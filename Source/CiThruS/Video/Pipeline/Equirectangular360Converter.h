#pragma once

#include "IImageFilter.h"
#include <vector>

// Converts a 360 cubemap into an equirectangular panorama
class Equirectangular360Converter : public IImageFilter
{
public:
	Equirectangular360Converter(const uint16_t& inputFrameWidth, const uint16_t& inputFrameHeight, const uint16_t& outputFrameWidth, const uint16_t& outputFrameHeight, const bool& bilinearFiltering, const uint8_t& threadCount);
	virtual ~Equirectangular360Converter();

	virtual void Process() override;

	virtual bool SetInput(const IImageSource* source) override;

	inline virtual uint8_t* const* GetOutput() const override { return &outputFrame_; }
	inline virtual const uint32_t* GetOutputSize() const override { return &outputSize_; }
	inline virtual std::string GetOutputFormat() const override { return outputFormat_; }

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

	uint8_t* const* inputFrame_;
	const uint32_t* inputSize_;

	uint8_t* outputFrame_;
	uint32_t outputSize_;
	std::string outputFormat_;

	uint16_t inputFrameWidth_;
	uint16_t inputFrameHeight_;

	uint16_t outputFrameWidth_;
	uint16_t outputFrameHeight_;

	bool bilinearFiltering_;

	uint8_t threadCount_;

	std::vector<CubeCoords> panoramaMap_;

	int EdgePixelIndexFromDirs(const FilterDirection& outDir, const FilterDirection& inDir,
		const CubeFace& face, const int& faceX, const int& faceY);
};
