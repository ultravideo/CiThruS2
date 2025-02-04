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
	panoramaMap_(outputFrameWidth_* outputFrameWidth_, CubeCoords())
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
			uint32_t posInMap = i + j * outputFrameWidth_;

			float theta = ((2.0f * i) / outputFrameWidth_ - 1.0f) * PI;
			float phi = ((2.0f * j) / outputFrameHeight_ - 1.0f) * halfPi;

			float x = cos(phi) * cos(theta);
			float y = sin(phi);
			float z = cos(phi) * sin(theta);

			if (theta >= -quarterPi && theta <= quarterPi)
			{
				panoramaMap_[posInMap].face = CubeFront;
			}
			else if (theta >= -threeQuartersPi && theta <= -quarterPi)
			{
				panoramaMap_[posInMap].face = CubeLeft;
				theta += halfPi;
			}
			else if (theta >= quarterPi && theta <= threeQuartersPi)
			{
				panoramaMap_[posInMap].face = CubeRight;
				theta -= halfPi;
			}
			else
			{
				panoramaMap_[posInMap].face = CubeBack;

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
				panoramaMap_[posInMap].face = CubeBottom;
			}
			else if (phi < -phi_threshold)
			{
				panoramaMap_[posInMap].face = CubeTop;
			}

			float axis;
			float cubeX;
			float cubeY;
			float angle;

			switch (panoramaMap_[posInMap].face)
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
				// This is only triggered if map[pos_in_map].face is an invalid value, which should never happen
				throw std::exception();
			}

			float sizeRatio = halfCubeSide / axis;

			cubeX *= sizeRatio;
			cubeY *= sizeRatio;

			// Rotate and translate
			panoramaMap_[posInMap].x = std::min(std::max(static_cast<double>(cubeX * cos(angle) - cubeY * sin(angle) + halfCubeSide), 0.0), static_cast<double>(inputFrameWidth_));
			panoramaMap_[posInMap].y = std::min(std::max(static_cast<double>(cubeX * sin(angle) + cubeY * cos(angle) + halfCubeSide), 0.0), static_cast<double>(inputFrameWidth_));
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
	if (*inputFrame_ == nullptr || *inputSize_ != inputFrameWidth_ * inputFrameHeight_ * 4)
	{
		return;
	}

	std::transform(
		panoramaMap_.begin(),
		panoramaMap_.end(),
		reinterpret_cast<std::array<uint8_t, 4>*>(outputFrame_),
		[&](const CubeCoords& faceCoord)
		{
			const CubeFace& face = faceCoord.face;

			if (!bilinearFiltering_)
			{
				// No filtering
				const int rgbaBufferIndex = ((int)faceCoord.y * FACES_IN_A_CUBE * inputFrameWidth_ + (int)faceCoord.x + face * inputFrameWidth_) * 4;

				return *reinterpret_cast<std::array<uint8_t, 4>*>(&(*inputFrame_)[rgbaBufferIndex]);
			}

			// Sample the cubemap textures with bilinear interpolation
			// The 0.5s here are to center the sampling areas so that each pixel's color is blended around its center
			int faceX = floor(faceCoord.x - 0.5);
			int faceY = floor(faceCoord.y - 0.5);

			const float dx = faceCoord.x - 0.5 - faceX;
			const float dy = faceCoord.y - 0.5 - faceY;

			// These variables are named as follows:
			// b = bottom, r = right, t = top, l = left
			// bl = bottom left and so on

			const float weightBl = (1.0f - dx) * (1.0f - dy);
			const float weightBr = dx * (1.0f - dy);
			const float weightTl = (1.0f - dx) * dy;
			const float weightTr = dx * dy;

			const bool rBoundaryCrossed = faceX + 1 >= inputFrameWidth_;
			const bool tBoundaryCrossed = faceY + 1 >= inputFrameHeight_;
			const bool lBoundaryCrossed = faceX < 0;
			const bool bBoundaryCrossed = faceY < 0;

			// Get the correct pixel indices for sampling
			int rgbaBufferIndexBl;
			int rgbaBufferIndexBr;
			int rgbaBufferIndexTl;
			int rgbaBufferIndexTr;

			if (!rBoundaryCrossed && !tBoundaryCrossed && !lBoundaryCrossed && !bBoundaryCrossed)
			{
				// If no boundaries were crossed, getting the sample pixels is simple because they're on the same face
				rgbaBufferIndexBl = ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + face * inputFrameWidth_) * 4;
				rgbaBufferIndexBr = ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + face * inputFrameWidth_) * 4;
				rgbaBufferIndexTl = ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + face * inputFrameWidth_) * 4;
				rgbaBufferIndexTr = ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + face * inputFrameWidth_) * 4;
			}
			else
			{
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
				if (rBoundaryCrossed)
				{
					FilterDirection outDir = FilterDirection::FilterRight;

					outDirBr = outDir;
					outDirTr = outDir;

					inDirBr = ENTRY_DIRECTIONS[faceBr][outDir];
					inDirTr = ENTRY_DIRECTIONS[faceTr][outDir];

					faceBr = ADJACENT_FACES[faceBr][outDir];
					faceTr = ADJACENT_FACES[faceTr][outDir];
				}

				if (tBoundaryCrossed)
				{
					FilterDirection outDir = FilterDirection::FilterUp;

					outDirTl = outDir;
					outDirTr = outDir;

					inDirTl = ENTRY_DIRECTIONS[faceTl][outDir];
					inDirTr = ENTRY_DIRECTIONS[faceTr][outDir];

					faceTl = ADJACENT_FACES[faceTl][outDir];
					faceTr = ADJACENT_FACES[faceTr][outDir];
				}

				if (lBoundaryCrossed)
				{
					FilterDirection outDir = FilterDirection::FilterLeft;

					outDirBl = outDir;
					outDirTl = outDir;

					inDirBl = ENTRY_DIRECTIONS[faceBl][outDir];
					inDirTl = ENTRY_DIRECTIONS[faceTl][outDir];

					faceBl = ADJACENT_FACES[faceBl][outDir];
					faceTl = ADJACENT_FACES[faceTl][outDir];
				}

				if (bBoundaryCrossed)
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
				rgbaBufferIndexBl =
					faceBl == face
					? ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + faceBl * inputFrameWidth_) * 4
					: EdgePixelIndexFromDirs(outDirBl, inDirBl, faceBl, faceX + 0, faceY + 0, FACES_IN_A_CUBE);
				rgbaBufferIndexBr =
					faceBr == face
					? ((faceY + 0) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + faceBr * inputFrameWidth_) * 4
					: EdgePixelIndexFromDirs(outDirBr, inDirBr, faceBr, faceX + 1, faceY + 0, FACES_IN_A_CUBE);
				rgbaBufferIndexTl =
					faceTl == face
					? ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 0) + faceTl * inputFrameWidth_) * 4
					: EdgePixelIndexFromDirs(outDirTl, inDirTl, faceTl, faceX + 0, faceY + 1, FACES_IN_A_CUBE);
				rgbaBufferIndexTr =
					faceTr == face
					? ((faceY + 1) * FACES_IN_A_CUBE * inputFrameWidth_ + (faceX + 1) + faceTr * inputFrameWidth_) * 4
					: EdgePixelIndexFromDirs(outDirTr, inDirTr, faceTr, faceX + 1, faceY + 1, FACES_IN_A_CUBE);
			}

			std::array<uint8_t, 4> outputRgba{};

			// Calculate the final interpolated color based on the weights and sampled pixels
			for (int color_channel = 0; color_channel < 4; color_channel++)
			{
				outputRgba[color_channel] =
					weightBl * (*inputFrame_)[rgbaBufferIndexBl + color_channel] +
					weightBr * (*inputFrame_)[rgbaBufferIndexBr + color_channel] +
					weightTl * (*inputFrame_)[rgbaBufferIndexTl + color_channel] +
					weightTr * (*inputFrame_)[rgbaBufferIndexTr + color_channel];
			}

			return outputRgba;
		});
}

bool Equirectangular360Converter::SetInput(const IImageSource* source)
{
	if (source->GetOutputFormat() != "rgba")
	{
		return false;
	}

	inputFrame_ = source->GetOutput();
	inputSize_ = source->GetOutputSize();

	return true;
}

int Equirectangular360Converter::EdgePixelIndexFromDirs(const FilterDirection& out_dir, const FilterDirection& in_dir,
	const CubeFace& face, const int& face_x, const int& face_y, const int& num_frames)
{
	// This function gets the correct texture edge pixel based on the "exit" and "entry" directions when crossing
	// the edge between two sides of the cubemap. This is needed to filter the cubemap edges correctly because
	// the cubemap sides are not always adjacent in the flat texture where they are stored

	int side_coord;

	// First map the pixel coordinates into a generic "side coordinate" that always goes clockwise along the texture edge
	switch (out_dir)
	{
	case FilterDirection::FilterDown:
		side_coord = (face_x + inputFrameWidth_) % inputFrameWidth_;
		break;

	case FilterDirection::FilterRight:
		side_coord = (face_y + inputFrameHeight_) % inputFrameHeight_;
		break;

	case FilterDirection::FilterUp:
		side_coord = (inputFrameWidth_ - (face_x + inputFrameWidth_) % inputFrameWidth_ - 1);
		break;

	case FilterDirection::FilterLeft:
		side_coord = (inputFrameHeight_ - (face_y + inputFrameHeight_) % inputFrameHeight_ - 1);
		break;

	default:
		throw std::invalid_argument("Invalid exit direction");
	}

	// Then calculate the pixel index on the other face beyond the edge using the clockwise side coordinate
	switch (in_dir)
	{
	case FilterDirection::FilterDown:
		return ((inputFrameWidth_ - side_coord - 1) + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterRight:
		return ((inputFrameHeight_ - side_coord - 1) * num_frames * inputFrameWidth_ + (inputFrameWidth_ - 1) + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterUp:
		return ((inputFrameHeight_ - 1) * num_frames * inputFrameWidth_ + side_coord + face * inputFrameWidth_) * 4;

	case FilterDirection::FilterLeft:
		return (side_coord * num_frames * inputFrameWidth_ + face * inputFrameWidth_) * 4;

	default:
		throw std::invalid_argument("Invalid entry direction");
	}
}
