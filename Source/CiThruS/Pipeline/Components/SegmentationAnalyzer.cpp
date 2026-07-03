#include "SegmentationAnalyzer.h"
#include "JsonLogger.h"
#include "Video/SegmentationController.h"

#include <algorithm>
#include <array>

SegmentationAnalyzer::SegmentationAnalyzer(const uint16_t& frameWidth, const uint16_t& frameHeight)
	: outputImageData_(nullptr), outputImageSize_(0), outputData_(nullptr), outputSize_(0), frameNumber_(0), inputWidth_(frameWidth), inputHeight_(frameHeight)
{
	outputImageSize_ = inputWidth_ * inputHeight_ * 2 * 4;
	outputImageData_ = new uint8_t[outputImageSize_];

	pixelVisited_ = new uint8_t[inputWidth_ * inputHeight_];

	GetInputPin<0>().Initialize(this, "rgba");
	GetInputPin<1>().Initialize(this, "segmentationMap");
	GetOutputPin<0>().Initialize(this, "rgba");
	GetOutputPin<1>().Initialize(this, "binary");
}

SegmentationAnalyzer::~SegmentationAnalyzer()
{
	delete[] outputImageData_;
	outputImageData_ = nullptr;
	outputImageSize_ = 0;

	delete[] outputData_;
	outputData_ = nullptr;
	outputSize_ = 0;

	delete[] pixelVisited_;
	pixelVisited_ = nullptr;

	GetOutputPin<0>().SetData(nullptr);
	GetOutputPin<0>().SetSize(0);
	GetOutputPin<1>().SetData(nullptr);
	GetOutputPin<1>().SetSize(0);
}

void SegmentationAnalyzer::Process()
{
	const uint8_t* inputData = GetInputPin<0>().GetData();
	uint32_t inputSize = GetInputPin<0>().GetSize();

	const std::vector<SegmentedObject>* segmentationMap = reinterpret_cast<const std::vector<SegmentedObject>*>(GetInputPin<1>().GetData());
	uint32_t segmentationMapSize = GetInputPin<1>().GetSize();

	if (!inputData || inputSize == 0 || !segmentationMap || segmentationMapSize != sizeof(const std::vector<SegmentedObject>*))
	{
		GetOutputPin<0>().SetData(nullptr);
		GetOutputPin<0>().SetSize(0);
		GetOutputPin<1>().SetData(nullptr);
		GetOutputPin<1>().SetSize(0);

		return;
	}

	delete[] outputData_;

	JsonLogData* results = new JsonLogData{};

	results->frameNumber = frameNumber_;
	results->objectsDetected = std::vector<JsonObjectDetection>();

	frameNumber_++;

	trackedObjects_.clear();

	for (int y = 0; y < inputHeight_; y++)
	{
		for (int x = 0; x < inputWidth_; x++)
		{
			float normalizedX = static_cast<float>(x) / inputWidth_;
			float normalizedY = static_cast<float>(y) / inputHeight_;

			// Assumes stencil value is in alpha channel
			uint8_t stencilValue = inputData[(x + y * inputWidth_) * 4 + 3];
			SegmentedObject objectData = (*segmentationMap)[stencilValue];

			if (stencilValue != 0)
			{
				TrackedObject* trackedObject = nullptr;

				auto findResult = trackedObjects_.find(stencilValue);

				if (findResult != trackedObjects_.end())
				{
					trackedObject = &findResult->second;
				}
				else
				{
					trackedObject = &trackedObjects_.insert(std::pair<int, TrackedObject>(stencilValue, TrackedObject{})).first->second;

					trackedObject->trackId = objectData.trackingId;
					trackedObject->classId = static_cast<int>(objectData.classId);
					trackedObject->stencilId = stencilValue;
					
					FColor maskColor = objectData.maskColor;

					trackedObject->maskColorR = maskColor.R;
					trackedObject->maskColorG = maskColor.G;
					trackedObject->maskColorB = maskColor.B;

					// Tracking ID of zero is used to signify that this object is not segmented by instance and thus can't be tracked as an individual object
					trackedObject->isCrowd = trackedObject->trackId == 0 ? 1 : 0;

					trackedObject->xMin = normalizedX;
					trackedObject->xMax = normalizedX;
					trackedObject->yMin = normalizedY;
					trackedObject->yMax = normalizedY;
				}

				if (normalizedX < trackedObject->xMin)
				{
					trackedObject->xMin = normalizedX;
				}

				if (normalizedX > trackedObject->xMax)
				{
					trackedObject->xMax = normalizedX;
				}

				if (normalizedY < trackedObject->yMin)
				{
					trackedObject->yMin = normalizedY;
				}

				if (normalizedY > trackedObject->yMax)
				{
					trackedObject->yMax = normalizedY;
				}

				// If any neighboring pixel has a different stencil value, this is an edge pixel.
				// Track edge pixels for simplifying polygon calculation later
				if (trackedObject->classId <= 80)
				{
					int exposedXSidesCount = 0;
					int exposedYSidesCount = 0;

					if (x == 0 || inputData[((x - 1) + y * inputWidth_) * 4 + 3] != stencilValue)
					{
						exposedXSidesCount++;
					}

					if (x == inputWidth_ - 1 || inputData[((x + 1) + y * inputWidth_) * 4 + 3] != stencilValue)
					{
						exposedXSidesCount++;
					}

					if (y == 0 || inputData[(x + (y - 1) * inputWidth_) * 4 + 3] != stencilValue)
					{
						exposedYSidesCount++;
					}

					if (y == inputHeight_ - 1 || inputData[(x + (y + 1) * inputWidth_) * 4 + 3] != stencilValue)
					{
						exposedYSidesCount++;
					}

					// Don't count one pixel thin parts as edge pixels because they mess up the polygon calculation later
					if (exposedXSidesCount == 1 || exposedYSidesCount == 1)
					{
						trackedObject->edgePixels.insert((static_cast<uint64_t>(x) << 32) | y);

						if (trackedObject->yMin == normalizedY)
						{
							trackedObject->firstEdgePixel = { x, y };
						}

						/*outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 0] = 0;
						outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 1] = 255;
						outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 2] = 0;
						outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 3] = 255;*/
					}
				}

				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 0] = trackedObject->maskColorR;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 1] = trackedObject->maskColorG;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 2] = trackedObject->maskColorB;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 3] = 255;
			}
			else
			{
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 0] = 0;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 1] = 0;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 2] = 0;
				outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 3] = 255;
			}

			outputImageData_[(x + y * inputWidth_) * 4 + 0] = inputData[(x + y * inputWidth_) * 4 + 0];
			outputImageData_[(x + y * inputWidth_) * 4 + 1] = inputData[(x + y * inputWidth_) * 4 + 1];
			outputImageData_[(x + y * inputWidth_) * 4 + 2] = inputData[(x + y * inputWidth_) * 4 + 2];
			outputImageData_[(x + y * inputWidth_) * 4 + 3] = 255;

			// Reset pixel visitation status from last frame
			pixelVisited_[x + y * inputWidth_] = false;
		}
	}

	for (auto it = trackedObjects_.begin(); it != trackedObjects_.end(); it++)
	{
		TrackedObject& trackedObject = it->second;

		results->objectsDetected.push_back(JsonObjectDetection{});

		JsonObjectDetection& detection = results->objectsDetected[results->objectsDetected.size() - 1];

		detection.classId = trackedObject.classId;
		detection.isCrowd = trackedObject.isCrowd;
		detection.maskColorR = trackedObject.maskColorR;
		detection.maskColorG = trackedObject.maskColorG;
		detection.maskColorB = trackedObject.maskColorB;
		detection.trackId = trackedObject.trackId;
		detection.xMin = trackedObject.xMin;
		detection.xMax = trackedObject.xMax;
		detection.yMin = trackedObject.yMin;
		detection.yMax = trackedObject.yMax;

		std::pair<int, int> currentPixel = trackedObject.firstEdgePixel;
		uint8_t entryDirection = 0;
		uint8_t oldEntryDirection = 0;
		uint8_t olderEntryDirection = 0;

		bool finished = false;

		while (!finished)
		{
			const int x = currentPixel.first;
			const int y = currentPixel.second;

			pixelVisited_[(x + y * inputWidth_)] = true;

			olderEntryDirection = oldEntryDirection;
			oldEntryDirection = entryDirection;

			finished = true;

			// Calculate concave hull polygon. Based on algorithms such as in the paper
			// "A. Moreira and M. Y. Santos, Concave hull: A k-nearest neighbours approach for the computation of the region occupied by a set of points"
			// Slightly simplified/modified to account for the fact that the points are pixels on a grid and thus have only 8 possible neighbors
			// However, this doesn't work correctly if the mask of a single object is split into two or more parts. It will only find one part and discard the others.
			// Including all detached parts in the polygon would require searching a much larger number of pixels and also probably check for intersections like in the paper,
			// so it would make this more complicated
			for (int i = 0; i < 7; i++)
			{
				if ((this->*tryFindNextPixelFromDirection_[entryDirection][i])(currentPixel, entryDirection, trackedObject, inputData))
				{
					finished = false;

					break;
				}
			}

			if (entryDirection != oldEntryDirection || detection.polygon.size() == 0)
			{
				if (entryDirection != olderEntryDirection || detection.polygon.size() == 0)
				{
					detection.polygon.push_back({ static_cast<float>(x) / inputWidth_, static_cast<float>(y) / inputHeight_ });
				}
				else
				{
					detection.polygon[detection.polygon.size() - 1] = { static_cast<float>(x) / inputWidth_, static_cast<float>(y) / inputHeight_ };
				}
			}
		}

		/*for (const std::pair<float, float> corner : detection.polygon)
		{
			const int& x = corner.first * inputWidth_;
			const int& y = corner.second * inputHeight_;

			outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 0] = 255;
			outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 1] = 0;
			outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 2] = 0;
			outputImageData_[inputWidth_ * inputHeight_ * 4 + (x + y * inputWidth_) * 4 + 3] = 255;
		}*/
	}

	outputData_ = reinterpret_cast<uint8_t*>(results);
	outputSize_ = sizeof(results);

	GetOutputPin<0>().SetData(outputImageData_);
	GetOutputPin<0>().SetSize(outputImageSize_);
	GetOutputPin<1>().SetData(outputData_);
	GetOutputPin<1>().SetSize(outputSize_);
}

bool SegmentationAnalyzer::CheckNegativeX(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x > 0
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x - 1) << 32) | y) != trackedObject.edgePixels.end()
		&& inputData[((x - 1) + y * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x - 1) + y * inputWidth_])
	{
		currentPixel = { x - 1, y };
		entryDirection = 4;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckPositiveX(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x < inputWidth_ - 1
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x + 1) << 32) | y) != trackedObject.edgePixels.end()
		&& inputData[((x + 1) + y * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x + 1) + y * inputWidth_])
	{
		currentPixel = { x + 1, y };
		entryDirection = 0;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckNegativeY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (y > 0
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x) << 32) | (y - 1)) != trackedObject.edgePixels.end()
		&& inputData[(x + (y - 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[x + (y - 1) * inputWidth_])
	{
		currentPixel = { x, y - 1 };
		entryDirection = 6;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckPositiveY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (y < inputHeight_ - 1
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x) << 32) | (y + 1)) != trackedObject.edgePixels.end()
		&& inputData[(x + (y + 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[x + (y + 1) * inputWidth_])
	{
		currentPixel = { x, y + 1 };
		entryDirection = 2;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckNegXNegY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x > 0
		&& y > 0
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x - 1) << 32) | (y - 1)) != trackedObject.edgePixels.end()
		&& inputData[((x - 1) + (y - 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x - 1) + (y - 1) * inputWidth_])
	{
		currentPixel = { x - 1, y - 1 };
		entryDirection = 5;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckPosXNegY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x < inputWidth_ - 1
		&& y > 0
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x + 1) << 32) | (y - 1)) != trackedObject.edgePixels.end()
		&& inputData[((x + 1) + (y - 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x + 1) + (y - 1) * inputWidth_])
	{
		currentPixel = { x + 1, y - 1 };
		entryDirection = 7;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckPosXPosY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x < inputWidth_ - 1
		&& y < inputHeight_ - 1
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x + 1) << 32) | (y + 1)) != trackedObject.edgePixels.end()
		&& inputData[((x + 1) + (y + 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x + 1) + (y + 1) * inputWidth_])
	{
		currentPixel = { x + 1, y + 1 };
		entryDirection = 1;

		return true;
	}

	return false;
}

bool SegmentationAnalyzer::CheckNegXPosY(std::pair<int, int>& currentPixel, uint8_t& entryDirection, const TrackedObject& trackedObject, const uint8_t* inputData) const
{
	const int& x = currentPixel.first;
	const int& y = currentPixel.second;

	if (x > 0
		&& y < inputHeight_ - 1
		&& trackedObject.edgePixels.find((static_cast<uint64_t>(x - 1) << 32) | (y + 1)) != trackedObject.edgePixels.end()
		&& inputData[((x - 1) + (y + 1) * inputWidth_) * 4 + 3] == trackedObject.stencilId
		&& !pixelVisited_[(x - 1) + (y + 1) * inputWidth_])
	{
		currentPixel = { x - 1, y + 1 };
		entryDirection = 3;

		return true;
	}

	return false;
}
