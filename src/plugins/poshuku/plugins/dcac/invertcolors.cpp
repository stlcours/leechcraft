/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "invertcolors.h"
#include <cmath>
#include <QImage>
#include <util/sys/cpufeatures.h>
#include <util/sll/intseq.h>
#include "effectscommon.h"

#ifdef SSE_ENABLED
#include "ssecommon.h"
#endif

namespace LeechCraft
{
namespace Poshuku
{
namespace DCAC
{
	namespace
	{
		uint64_t CombineGray (uint64_t r, uint64_t g, uint64_t b)
		{
			return r * 11 + g * 16 + b * 5;
		}

		uint64_t GetGrayDefault (const QImage& image)
		{
			uint64_t r = 0, g = 0, b = 0;

			const auto height = image.height ();
			const auto width = image.width ();

			for (int y = 0; y < height; ++y)
			{
				const auto scanline = reinterpret_cast<const QRgb*> (image.scanLine (y));
				for (int x = 0; x < width; ++x)
				{
					auto color = scanline [x];
					r += qRed (color);
					g += qGreen (color);
					b += qBlue (color);
				}
			}
			return CombineGray (r, g, b);
		}

		void InvertRgbDefault (QImage& image)
		{
			const auto height = image.height ();
			const auto width = image.width ();

			for (int y = 0; y < height; ++y)
			{
				const auto scanline = reinterpret_cast<QRgb*> (image.scanLine (y));
				for (int x = 0; x < width; ++x)
					scanline [x] ^= 0x00ffffff;
			}
		}

#ifdef SSE_ENABLED
		__attribute__ ((target ("avx")))
		void InvertRgbAVX (QImage& image)
		{
			constexpr auto alignment = 32;

			const auto height = image.height ();
			const auto width = image.width ();

			const __m256 xorMask = _mm256_castsi256_ps (_mm256_set1_epi32 (0x00ffffff));

			for (int y = 0; y < height; ++y)
			{
				uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;
				auto handler = [scanline] (int i)
				{
					*reinterpret_cast<QRgb*> (&scanline [i]) ^= 0x00ffffff;
				};
				constexpr auto unroll = 8;
				HandleLoopBegin<alignment * unroll> (scanline, width, x, bytesCount, handler);

				for (; x < bytesCount; x += alignment * unroll)
				{
					__m256i p0 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 0));
					__m256i p1 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 1));
					__m256i p2 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 2));
					__m256i p3 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 3));
					__m256i p4 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 4));
					__m256i p5 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 5));
					__m256i p6 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 6));
					__m256i p7 = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x + alignment * 7));
					p0 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p0), xorMask));
					p1 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p1), xorMask));
					p2 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p2), xorMask));
					p3 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p3), xorMask));
					p4 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p4), xorMask));
					p5 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p5), xorMask));
					p6 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p6), xorMask));
					p7 = _mm256_castps_si256 (_mm256_xor_ps (_mm256_castsi256_ps (p7), xorMask));
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 0), p0);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 1), p1);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 2), p2);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 3), p3);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 4), p4);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 5), p5);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 6), p6);
					_mm256_store_si256 (reinterpret_cast<__m256i*> (scanline + x + alignment * 7), p7);
				}

				HandleLoopEnd (width, x, handler);
			}
		}

		__attribute__ ((target ("sse4")))
		uint64_t GetGraySSE4 (const QImage& image)
		{
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;

			__m128i sum = _mm_setzero_si128 ();

			const auto height = image.height ();
			const auto width = image.width ();

			const __m128i pixel1msk = MakeMask<128, 3, 0> ();
			const __m128i pixel2msk = MakeMask<128, 7, 4> ();
			const __m128i pixel3msk = MakeMask<128, 11, 8> ();
			const __m128i pixel4msk = MakeMask<128, 15, 12> ();

			constexpr auto alignment = 16;

			for (int y = 0; y < height; ++y)
			{
				const uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;

				auto handler = [&r, &g, &b, scanline] (int i)
				{
					auto color = *reinterpret_cast<const QRgb*> (&scanline [i]);
					r += qRed (color);
					g += qGreen (color);
					b += qBlue (color);
				};
				HandleLoopBegin<alignment> (scanline, width, x, bytesCount, handler);

#if __clang__
				#pragma unroll(8)
#endif
				for (; x < bytesCount; x += alignment)
				{
					const __m128i fourPixels = _mm_load_si128 (reinterpret_cast<const __m128i*> (scanline + x));

					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel1msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel2msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel3msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel4msk));
				}

				HandleLoopEnd (width, x, handler);
			}

			r += _mm_extract_epi32 (sum, 2);
			g += _mm_extract_epi32 (sum, 1);
			b += _mm_extract_epi32 (sum, 0);

			return CombineGray (r, g, b);
		}

		__attribute__ ((target ("avx2")))
		uint64_t GetGrayAVX2 (const QImage& image)
		{
			uint32_t r = 0;
			uint32_t g = 0;
			uint32_t b = 0;

			__m256i sum = _mm256_setzero_si256 ();

			const auto height = image.height ();
			const auto width = image.width ();

			const __m256i ppair1mask = MakeMask<256, 3, 0> ();
			const __m256i ppair2mask = MakeMask<256, 7, 4> ();
			const __m256i ppair3mask = MakeMask<256, 11, 8> ();
			const __m256i ppair4mask = MakeMask<256, 15, 12> ();

			constexpr auto alignment = 32;

			for (int y = 0; y < height; ++y)
			{
				const uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;

				auto handler = [&r, &g, &b, scanline] (int i)
				{
					auto color = *reinterpret_cast<const QRgb*> (&scanline [i]);
					r += qRed (color);
					g += qGreen (color);
					b += qBlue (color);
				};
				HandleLoopBegin<alignment> (scanline, width, x, bytesCount, handler);

				for (; x < bytesCount; x += alignment)
				{
					const __m256i eightPixels = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x));

					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair1mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair2mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair3mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair4mask));
				}

				HandleLoopEnd (width, x, handler);
			}

			r += _mm256_extract_epi32 (sum, 2);
			g += _mm256_extract_epi32 (sum, 1);
			b += _mm256_extract_epi32 (sum, 0);
			r += _mm256_extract_epi32 (sum, 6);
			g += _mm256_extract_epi32 (sum, 5);
			b += _mm256_extract_epi32 (sum, 4);

			return CombineGray (r, g, b);
		}
#endif

		uint64_t GetGray (const QImage& image)
		{
#ifdef SSE_ENABLED
			static const auto ptr = Util::CpuFeatures::Choose ({
						{ Util::CpuFeatures::Feature::AVX2, &GetGrayAVX2 },
						{ Util::CpuFeatures::Feature::SSE41, &GetGraySSE4 }
					},
					&GetGrayDefault);

			return ptr (image);
#else
			return GetGrayDefault (image);
#endif
		}

		void InvertRgb (QImage& image)
		{
			InvertRgbDefault (image);
		}
	}

	bool InvertColors (QImage& image, int threshold)
	{
		const auto height = image.height ();
		const auto width = image.width ();

		const auto sourceGraySum = threshold ?
				(GetGray (image) / (width * height * 32)) :
				0;
		const auto shouldInvert = !threshold || sourceGraySum >= static_cast<uint64_t> (threshold);

		if (shouldInvert)
			InvertRgb (image);

		return shouldInvert;
	}
}
}
}
