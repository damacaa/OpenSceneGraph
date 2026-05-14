#pragma once
#include <osgGA/OrbitManipulator>
#include <algorithm>

// Camera manipulator with consistent feel regardless of scene/node size.
//
// Problems with the stock OrbitManipulator:
//   • Pan speed = 0.3 * _distance → after focusing a tiny node _distance is
//     small and panning becomes impossibly slow.
//   • Scroll zoom = _distance * factor → tiny-node selections leave _distance
//     small, so each scroll barely moves.
//
// Fixes:
//   • Pan (middle button) clamps the effective distance to MIN_PAN_DIST.
//   • Scroll uses max(proportional, MIN_ZOOM_STEP) for the same reason.
//   • Right-drag rotates (default OrbitManipulator behaviour, unchanged).
//   • Throw (inertia on release) is disabled for tighter control.
//   • Vertical axis is fixed (Z-up) to avoid barrel-roll accidents.

class ConsistentManipulator : public osgGA::OrbitManipulator
{
public:
	// World-unit thresholds for unscaled FBX imports.
	static constexpr double MIN_PAN_DIST = 0.001; // never slower than this for pan
	static constexpr double MIN_ZOOM_STEP = 0.00001;  // never smaller than this per scroll tick
	static constexpr double ZOOM_FACTOR = 0.12;   // proportional portion (12 % per tick)

	ConsistentManipulator()
	{
		setMinimumDistance(0.001, false); // absolute, not relative to model size
		setVerticalAxisFixed(true);
		setAllowThrow(false);
	}

	// Call this when the export axis setting changes so the orbit
	// vertical matches the model's native up direction.
	void setModelIsYUp(bool yUp) { _yUp = yUp; }

	void setNode(osg::Node* node) override
	{
		osgGA::OrbitManipulator::setNode(node);
		setMinimumDistance(0.001, false);
	}

protected:
	bool _yUp = false;

	// Left-drag orbits — yaw axis follows the model's native up direction.
	bool performMovementLeftMouseButton(const double td,
		const double dx, const double dy) override
	{
		if (getVerticalAxisFixed())
		{
			const osg::Vec3f up = _yUp ? osg::Vec3f(0.f, 1.f, 0.f) : osg::Vec3f(0.f, 0.f, 1.f);
			// Clamp pitch so the camera never reaches the poles, which causes the
			// fixVerticalAxis flip-over correction to fire and produce rapid jitter.
			static constexpr double MAX_ELEV = 1.54;  // ~88 degrees in radians
			osg::Vec3d forward = _rotation * osg::Vec3d(0.0, 0.0, -1.0);
			double elev = std::asin(osg::clampBetween(forward * osg::Vec3d(up), -1.0, 1.0));
			double clampedDy = osg::clampBetween((double)dy, -MAX_ELEV - elev, MAX_ELEV - elev);
			rotateWithFixedVertical((float)dx, (float)clampedDy, up);
		}
		else
			rotateTrackball(_ga_t0->getXnormalized(), _ga_t0->getYnormalized(),
				_ga_t1->getXnormalized(), _ga_t1->getYnormalized(),
				getThrowScale(td));
		return true;
	}

	// Pan (middle button) — clamped minimum effective distance
	bool performMovementMiddleMouseButton(const double td,
		const double dx, const double dy) override
	{
		float scale = -0.3f * (float)std::max(_distance, MIN_PAN_DIST) * getThrowScale(td);
		panModel((float)(dx * scale), (float)(dy * scale));
		return true;
	}

	// Right drag also pans (consistent with most DCC tools)
	bool performMovementRightMouseButton(const double td,
		const double dx, const double dy) override
	{
		float scale = -0.3f * (float)std::max(_distance, MIN_PAN_DIST) * getThrowScale(td);
		panModel((float)(dx * scale), (float)(dy * scale));
		return true;
	}

	// Scroll wheel — fixed minimum step so close-up zooming isn't glacial
	bool handleMouseWheel(const osgGA::GUIEventAdapter& ea,
		osgGA::GUIActionAdapter& us) override
	{
		double step = std::max(MIN_ZOOM_STEP, _distance * ZOOM_FACTOR);
		auto sm = ea.getScrollingMotion();
		if (sm == osgGA::GUIEventAdapter::SCROLL_UP)
			_distance = std::max(getMinimumDistance(), _distance - step);
		else if (sm == osgGA::GUIEventAdapter::SCROLL_DOWN)
			_distance += step;
		else
			return false;
		us.requestRedraw();
		return true;
	}
};
