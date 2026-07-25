#include "helperFunc.hpp"

const LocationConf* getMatchingLocation(const std::vector<LocationConf>& locations, const std::string& path) {
	const LocationConf* bestMatch = NULL;
	size_t bestMatchLength = 0;

	for (std::vector<LocationConf>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
		const LocationConf& location = *it;
		const std::string& locationPath = location.getPath();

		if (path.compare(0, locationPath.length(), locationPath) == 0) {
			if (locationPath.length() > bestMatchLength) {
				bestMatch = &location;
				bestMatchLength = locationPath.length();
			}
		}
	}

	return bestMatch;
}