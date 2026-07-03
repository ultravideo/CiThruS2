#pragma once

#include "Pipeline/Internal/PipelineFilter.h"

#include <unordered_map>
#include <unordered_set>

class CITHRUS_API SegmentationAnalyzer : public PipelineFilter<2, 2>
{
public:
	SegmentationAnalyzer(const uint16_t& frameWidth, const uint16_t& frameHeight);
	virtual ~SegmentationAnalyzer();

	virtual void Process() override;

protected:
	struct TrackedObject
	{
		// These are what were chosen for the VCM dataset
		int classId; // COCO class ID
		float xMin; // Bounding box left side X coordinate normalized to 0.0-1.0 range
		float xMax; // Bounding box right side X coordinate normalized to 0.0-1.0 range
		float yMin; // Bounding box top side Y coordinate normalized to 0.0-1.0 range
		float yMax; // Bounding box bottom side Y coordinate normalized to 0.0-1.0 range
		int trackId; // ID unique to every individual tracked object
		uint8_t maskColorR; // R component of this object's color in the mask image
		uint8_t maskColorG; // G component of this object's color in the mask image
		uint8_t maskColorB; // B component of this object's color in the mask image
		int isCrowd; // Bool for whether this object is actually multiple overlapping objects, currently unused
		std::unordered_set<uint64_t> edgePixels; // Pixels on the edges of this object's mask
		std::pair<int, int> firstEdgePixel; // The topmost edge pixel, needs to be tracked to traverse the edge in a clockwise direction
		int stencilId; // The stencil value used for this object in the current image
	};

	uint8_t* outputImageData_;
	uint32_t outputImageSize_;
	uint8_t* outputData_;
	uint32_t outputSize_;

	uint32_t frameNumber_;

	uint16_t inputWidth_;
	uint16_t inputHeight_;

	uint8_t* pixelVisited_;

	std::unordered_map<int, TrackedObject> trackedObjects_;

	bool CheckNegativeX(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckPositiveX(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckNegativeY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckPositiveY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckNegXNegY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckPosXNegY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckPosXPosY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;
	bool CheckNegXPosY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const;

	bool(SegmentationAnalyzer::* tryFindNextPixelFromDirection_[8][7])(std::pair<int, int>&, uint8_t&, const TrackedObject&, const uint8_t*) const =
	{
		{
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY
		},
		{
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX
		},
		{
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY
		},
		{
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY
		},
		{
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY
		},
		{
			&SegmentationAnalyzer::CheckPositiveY,
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX
		},
		{
			&SegmentationAnalyzer::CheckNegXPosY,
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY
		},
		{
			&SegmentationAnalyzer::CheckNegativeX,
			&SegmentationAnalyzer::CheckNegXNegY,
			&SegmentationAnalyzer::CheckNegativeY,
			&SegmentationAnalyzer::CheckPosXNegY,
			&SegmentationAnalyzer::CheckPositiveX,
			&SegmentationAnalyzer::CheckPosXPosY,
			&SegmentationAnalyzer::CheckPositiveY
		}
	};
};
