#pragma once

#include "Pipeline/Internal/PipelineFilter.h"
#include <vector>

// Converts a 360 cubemap into an equirectangular panorama
class Equirectangular360Converter : public PipelineFilter<1, 1>
{
public:
	Equirectangular360Converter(
		const uint16_t& inputFrameWidth, const uint16_t& inputFrameHeight,
		const uint16_t& outputFrameWidth, const uint16_t& outputFrameHeight,
		const bool& bilinearFiltering);
	virtual ~Equirectangular360Converter();

	virtual void Process() override;
	virtual void OnInputPinsConnected() override;

protected:
	enum FilterDirection : uint8_t { FilterDown, FilterRight, FilterUp, FilterLeft };
	enum CubeFace : uint32_t { CubeTop, CubeLeft, CubeFront, CubeRight, CubeBack, CubeBottom };

	struct CubeCoords
	{
		CubeFace face;
		uint16_t x;
		uint16_t y;

		uint32_t indexBl;
		uint32_t indexBr;
		uint32_t indexTl;
		uint32_t indexTr;

		float weightBl;
		float weightBr;
		float weightTl;
		float weightTr;
	};

	uint8_t* outputData_;
	uint32_t outputSize_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	bool bilinearFiltering_;

	std::vector<CubeCoords> panoramaMap_;

	int EdgePixelIndexFromDirs(const FilterDirection& outDir, const FilterDirection& inDir,
		const CubeFace& face, const int& faceX, const int& faceY);
};
