struct compareDrawCommands {
	Camera* camera;
	int alpha_mode;

	compareDrawCommands(Camera* cam, int alpha_mode) : camera(cam), alpha_mode(alpha_mode) {
	}

	bool operator()(const sDrawCommand& a, const sDrawCommand& b) const {
		if (alpha_mode == (eAlphaMode::BLEND || eAlphaMode::MASK)) {
			return compareTranslucent(a, b);
		}
		else if (alpha_mode == eAlphaMode::NO_ALPHA) {
			return compareOpaque(a, b);
		}
		else {
			return compareOther(a, b);
		}
	}

	bool compareTranslucent(const sDrawCommand& a, const sDrawCommand& b) const {
		// Render based on distance from camera - translucent objects are rendered from furthest to nearest
		float distanceA = camera->eye.distance(Vector3f(a.model.m[12], a.model.m[13], a.model.m[14]));
		float distanceB = camera->eye.distance(Vector3f(b.model.m[12], b.model.m[13], b.model.m[14]));
		return distanceA > distanceB;
	}

	bool compareOpaque(const sDrawCommand& a, const sDrawCommand& b) const {
		// Render based on distance from camera - opaque objects are rendered from neareast to furthest
		float distanceA = camera->eye.distance(Vector3f(a.model.m[12], a.model.m[13], a.model.m[14]));
		float distanceB = camera->eye.distance(Vector3f(b.model.m[12], b.model.m[13], b.model.m[14]));
		return distanceA < distanceB;
	}

	bool compareOther(const sDrawCommand& a, const sDrawCommand& b) const {
		throw std::runtime_error("Alpha mode not supported");
	}
};
