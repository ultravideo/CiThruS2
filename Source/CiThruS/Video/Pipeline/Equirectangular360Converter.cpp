#include "Equirectangular360Converter.h"

#include <algorithm>
#include <stdexcept>
#include <array>

const int FACES_IN_A_CUBE = 6;

Equirectangular360Converter::Equirectangular360Converter(
	const uint16_t& inputFrameWidth, const uint16_t& inputFrameHeight,
	const uint16_t& outputFrameWidth, const uint16_t& outputFrameHeight,
	const bool& bilinearFiltering, const uint8_t& threadCount)
	:
	inputFrameWidth_(inputFrameWidth), inputFrameHeight_(inputFrameHeight),
	outputFrameWidth_(outputFrameWidth), outputFrameHeight_(outputFrameHeight),
	bilinearFiltering_(bilinearFiltering), threadCount_(threadCount),
	panoramaMap_(outputFrameWidth_* outputFrameHeight_, CubeCoords())
{
	float halfPi = PI / 2.0f;
	float quarterPi = PI / 4.0f;
	float threeQuartersPi = PI * 3.0f / 4.0f;

	float halfCubeSide = inputFrameWidth_ / 2.0f;

	// Precalculate a map of where each panorama pixel is on the cubemap
	for (uint32_t i = 0; i < outputFrameWidth_; i++)
	{
		for (uint32_t j = 0; j < outputFrameHeight_; j++)
		{
			float theta = ((2.0f * i) / outputFrameWidth_ - 1.0f) * PI;
			float phi = ((2.0f * j) / outputFrameHeight_ - 1.0f) * halfPi;

			float x = cos(phi) * cos(theta);
			float y = sin(phi);
			float z = cos(phi) * sin(theta);

			CubeFace face;

			if (theta >= -quarterPi && theta <= quarterPi)
			{
				face = CubeFront;
			}
			else if (theta >= -threeQuartersPi && theta <= -quarterPi)
			{
				face = CubeLeft;
				theta += halfPi;
			}
			else if (theta >= quarterPi && theta <= threeQuartersPi)
			{
				face = CubeRight;
				theta -= halfPi;
			}
			else
			{
				face = CubeBack;

				if (theta > 0.0)
				{
					theta -= PI;
				}
				else
				{
					theta += PI;
				}
			}

			float phi_threshold = atan2(1.0f, 1.0f / cos(theta));

			if (phi > phi_threshold)
			{
				face = CubeBottom;
			}
			else if (phi < -phi_threshold)
			{
				face = CubeTop;
			}

			float axis;
			float cubeX;
			float cubeY;
			float angle;

			switch (face)
			{
			case CubeTop:
				axis = y;
				cubeX = z;
				cubeY = x;
				angle = PI;
				break;

			case CubeBottom:
				axis = y;
				cubeX = x;
				cubeY = z;
				angle = -halfPi;
				break;

			case CubeLeft:
				axis = z;
				cubeX = x;
				cubeY = y;
				angle = PI;
				break;

			case CubeRight:
				axis = z;
				cubeX = y;
				cubeY = x;
				angle = halfPi;
				break;

			case CubeFront:
				axis = x;
				cubeX = z;
				cubeY = y;
				angle = 0.0;
				break;

			case CubeBack:
				axis = x;
				cubeX = y;
				cubeY = z;
				angle = -halfPi;
				break;

			default:
				// This is only triggered if face is an invalid value, which should never happen
				throw std::exception();
			}

			float sizeRatio = halfCubeSide / axis;

			cubeX *= sizeRatio;
			cubeY *= sizeRatio;

			// Rotate and translate
			const float rawX = cubeX * cos(angle) - cubeY * sin(angle) + halfCubeSide;
			const float rawY = cubeX * sin(angle) + cubeY * cos(angle) + halfCubeSide;

			// Sample the cubemap textures with bilinear interpolation
			// The 0.5s here are to center the sampling areas so that each pixel's color is blended around its center
			const int faceX = floor(rawX - 0.5);
			const int faceY = floor(rawY - 0.5);

			const float dx = rawX - 0.5 - faceX;
			const float dy = rawY - 0.5 - faceY;

			// These variables are named as follows:
			// b = bottom, r = right, t = top, l = left
			// bl = bottom left and so on

			const float weightBl = (1.0f - dx) * (1.0f - dy);
			const float weightBr = dx * (1.0f - dy);
			const float weightTl = (1.0f - dx) * dy;
			const float weightTr = dx * dy;

			// Adjacent faces for each face in order of the direction enum
			const static CubeFace ADJACENT_FACES[6][4] =
			{
				{ CubeFace::CubeBack,  CubeFace::CubeRight, CubeFace::CubeFront,   CubeFace::CubeLeft  },
				{ CubeFace::CubeTop,   CubeFace::CubeFront, CubeFace::CubeBottom,  CubeFace::CubeBack  },
				{ CubeFace::CubeTop,   CubeFace::CubeRight, CubeFace::CubeBottom,  CubeFace::CubeLeft  },
				{ CubeFace::CubeTop,   CubeFace::CubeBack,  CubeFace::CubeBottom,  CubeFace::CubeFront },
				{ CubeFace::CubeTop,   CubeFace::CubeLeft,  CubeFace::CubeBottom,  CubeFace::CubeRight },
				{ CubeFace::CubeFront, CubeFace::CubeRight, CubeFace::CubeBack,    CubeFace::CubeLeft  }
			};

			// The directions from which each face is entered from when coming from the corresponding adjacent face
			const static FilterDirection ENTRY_DIRECTIONS[6][4] =
			{
				{ FilterDirection::FilterDown,  FilterDirection::FilterDown, FilterDirection::FilterDown,  FilterDirection::FilterDown  },
				{ FilterDirection::FilterLeft,  FilterDirection::FilterLeft, FilterDirection::FilterLeft,  FilterDirection::FilterRight },
				{ FilterDirection::FilterUp,    FilterDirection::FilterLeft, FilterDirection::FilterDown,  FilterDirection::FilterRight },
				{ FilterDirection::FilterRight, FilterDirection::FilterLeft, FilterDirection::FilterRight, FilterDirection::FilterRight },
				{ FilterDirection::FilterDown,  FilterDirection::FilterLeft, FilterDirection::FilterUp,    FilterDirection::FilterRight },
				{ FilterDirection::FilterUp,    FilterDirection::FilterUp,   FilterDirection::FilterUp,    FilterDirection::FilterUp    }
			};

			// Get the correct pixel indices for sampling
			CubeFace faceBl = face;
			CubeFace faceBr = face;
			CubeFace faceTl = face;
			CubeFace faceTr = face;

			FilterDirection inDirBl;
			FilterDirection inDirBr;
			FilterDirection inDirTl;
			FilterDirection inDirTr;

			FilterDirection outDirBl;
			FilterDirection outDirBr;
			FilterDirection outDirTl;
			FilterDirection outDirTr;

			// Check if any of the sample pixels crosses over onto another side of the cubemap
			if (faceX + 1 >= inputFrameWidth_)
			{
				FilterDirection outDir = FilterDirection::FilterRight;

				outDirBr = outDir;
				outDirTr = outDir;

				inDirBr = ENTRY_DIRECTIONS[faceBr][outDir];
				inDirTr = ENTRY_DIRECTIONS[faceTr][outDir];

				faceBr = ADJACENT_FACES[faceBr][outDir];
				faceTr = ADJACENT_FACES[faceTr][outDir];
			}

			if (faceY + 1 >= inputFrameHeight_)
			{
				FilterDirection outDir = FilterDirection::FilterUp;

				outDirTl = outDir;
				outDirTr = outDir;

				inDirTl = ENTRY_DIRECTIONS[faceTl][outDir];
				inDirTr = ENTRY_DIRECTIONS[faceTr][outDir];

				faceTl = ADJACENT_FACES[faceTl][outDir];
				faceTr = ADJACENT_FACES[faceTr][outDir];
			}

			if (faceX < 0)
			{
				FilterDirection outDir = FilterDirection::FilterLeft;

				outDirBl = outDir;
				outDirTl = outDir;

				inDirBl = ENTRY_DIRECTIONS[faceBl][outDir];
				inDirTl = ENTRY_DIRECTIONS[faceTl][outDir];

				faceBl = ADJACENT_FACES[faceBl][outDir];
				faceTl = ADJACENT_FACES[faceTl][outDir];
			}

			if (faceY < 0)
			{
				FilterDirection outDir = FilterDirection::FilterDown;

				outDirBl = outDir;
				outDirBr = outDir;

				inDirBl = ENTRY_DIRECTIONS[faceBl][outDir];
				inDirBr = ENTRY_DIRECTIONS[faceBr][outDir];

				faceBl = ADJACENT_FACES[faceBl][outDir];
				faceBr = ADJACENT_FACES[faceBr][outDir];
			}

			// If any sample pixel crossed over to another cubemap face, calculate the correct pixel there, otherwise
			// get the correct pixel on this face
			const uint32_t indexBl =
				faceBl == face
				? ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + faceBl * inputFrameWidth_) * 4
				: EdgePixelIndexFromDirs(outDirBl, inDirBl, faceBl, faceX + 0, faceY + 0);
			const uint32_t indexBr =
				faceBr == face
				? ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + faceBr * inputFrameWidth_) * 4
				: EdgePixelIndexFromDirs(outDirBr, inDirBr, faceBr, faceX + 1, faceY + 0);
			const uint32_t indexTl =
				faceTl == face
				? ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + faceTl * inputFrameWidth_) * 4
				: EdgePixelIndexFromDirs(outDirTl, inDirTl, faceTl, faceX + 0, faceY + 1);
			const uint32_t indexTr =
				faceTr == face
				? ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + faceTr * inputFrameWidth_) * 4
				: EdgePixelIndexFromDirs(outDirTr, inDirTr, faceTr, faceX + 1, faceY + 1);

			panoramaMap_[i + j * static_cast<size_t>(outputFrameWidth_)] =
			{
				face,
				static_cast<uint16_t>(std::min(std::max(static_cast<int>(rawX), 0), inputFrameWidth_ - 1)),
				static_cast<uint16_t>(std::min(std::max(static_cast<int>(rawY), 0), inputFrameHeight_ - 1)),
				indexBl,
				indexBr,
				indexTl,
				indexTr,
				weightBl,
				weightBr,
				weightTl,
				weightTr
			};
		}
	}

	outputSize_ = outputFrameWidth_ * outputFrameHeight_ * 4;
	outputFrame_ = new uint8_t[outputSize_];
}

Equirectangular360Converter::~Equirectangular360Converter()
{
	delete[] outputFrame_;
	outputFrame_ = nullptr;
	outputSize_ = 0;
}

void Equirectangular360Converter::Process()
{
	if (*inputFrame_ == nullptr || *inputSize_ != inputFrameWidth_ * inputFrameHeight_ * FACES_IN_A_CUBE * 4)
	{
		return;
	}

	if (!bilinearFiltering_)
	{
		std::transform(
			panoramaMap_.begin(),
			panoramaMap_.end(),
			reinterpret_cast<std::array<uint8_t, 4>*>(outputFrame_),
			[&](const CubeCoords& faceCoord)
			{
				return *reinterpret_cast<std::array<uint8_t, 4>*>(*inputFrame_ + (faceCoord.y * FACES_IN_A_CUBE * inputFrameWidth_ + faceCoord.x + faceCoord.face * inputFrameWidth_) * 4);
			});
	}
	else
	{
		std::transform(
			panoramaMap_.begin(),
			panoramaMap_.end(),
			reinterpret_cast<std::array<uint8_t, 4>*>(outputFrame_),
			[&](const CubeCoords& faceCoord)
			{
				std::array<uint8_t, 4> outputRgba{};

				// Calculate the final interpolated color based on the weights and sampled pixels
				for (int color_channel = 0; color_channel < 4; color_channel++)
				{
					outputRgba[color_channel] =
						faceCoord.weightBl * (*inputFrame_)[faceCoord.indexBl + color_channel] +
						faceCoord.weightBr * (*inputFrame_)[faceCoord.indexBr + color_channel] +
						faceCoord.weightTl * (*inputFrame_)[faceCoord.indexTl + color_channel] +
						faceCoord.weightTr * (*inputFrame_)[faceCoord.indexTr + color_channel];
				}

				return outputRgba;
			});
	}
}

bool Equirectangular360Converter::SetInput(const IImageSource* source)
{
	std::string inputFormat = source->GetOutputFormat();

	if (inputFormat == "rgba" && inputFormat != "bgra")
	{
		return false;
	}

	outputFormat_ = inputFormat;
	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}

int Equirectangular360Converter::EdgePixelIndexFromDirs(const FilterDirection& outDir, const FilterDirection& inDir,
	const CubeFace& face, const int& faceX, const int& faceY)
{
	// This function gets the correct texture edge pixel based on the "exit" and "entry" directions when crossing
	// the edge between two sides of the cubemap. This is needed to filter the cubemap edges correctly because
	// the cubemap sides are not always adjacent in the flat texture where they are stored

	int side_coord;

	// First map the pixel coordinates into a generic "side coordinate" that always goes clockwise along the texture edge
	switch (outDir)
	{
	case FilterDirection::FilterDown:
		side_coord = (faceX + inputFrameWidth_) % inputFrameWidth_;
		break;

	case FilterDirection::FilterRight:
		side_coord = (faceY + inputFrameHeight_) % inputFrameHeight_;
		break;

	case FilterDirection::FilterUp:
		side_coord = (inputFrameWidth_ - (faceX + inputFrameWidth_) % inputFrameWidth_ - 1);
		break;

	case FilterDirection::FilterLeft:
		side_coord = (inputFrameHeight_ - (faceY + inputFrameHeight_) % inputFrameHeight_ - 1);
		break;

	default:
		throw std::invalid_argument("Invalid exit direction");
	}

	// Then calculate the pixel index on the other face beyond the edge using the clockwise side coordinate
	switch (inDir)
	{
	case FilterDirection::FilterDown:
		return ((inputFrameWidth_ - side_coord - 1) + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterRight:
		return ((inputFrameHeight_ - side_coord - 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (inputFrameWidth_ - 1) + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterUp:
		return ((inputFrameHeight_ - 1) * FACES_IN_A_CUBE * inputFrameWidth_ + side_coord + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterLeft:
		return (side_coord * FACES_IN_A_CUBE * inputFrameWidth_ + face * inputFrameWidth_) * 4;

	default:
		throw std::invalid_argument("Invalid entry direction");
	}
}
