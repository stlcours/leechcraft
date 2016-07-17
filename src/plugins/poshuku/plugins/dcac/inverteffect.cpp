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

#include "inverteffect.h"
#include <QPainter>
#include <QtDebug>
#include <qwebview.h>

#ifdef Q_PROCESSOR_X86_64
#include <tmmintrin.h>
#include <immintrin.h>
#include <cpuid.h>
#endif

namespace LeechCraft
{
namespace Poshuku
{
namespace DCAC
{
	InvertEffect::InvertEffect (QWebView *view)
	: QGraphicsEffect { view }
	{
	}

	void InvertEffect::SetThreshold (int threshold)
	{
		if (threshold == Threshold_)
			return;

		Threshold_ = threshold;
		update ();
	}

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

		void InvertDefault (QImage& image)
		{
			const auto height = image.height ();
			const auto width = image.width ();

			for (int y = 0; y < height; ++y)
			{
				const auto scanline = reinterpret_cast<QRgb*> (image.scanLine (y));
				for (int x = 0; x < width; ++x)
				{
					auto& color = scanline [x];
					color &= 0x00ffffff;
					color = uint32_t { 0xffffffff } - color;
				}
			}
		}

#ifdef Q_PROCESSOR_X86_64
		template<int Alignment, typename F>
		void HandleLoopBegin (const uchar * const scanline, int width, int& x, int& bytesCount, F&& f)
		{
			const auto beginUnaligned = (scanline - static_cast<const uchar*> (nullptr)) % Alignment;
			bytesCount = width * 4;
			if (!beginUnaligned)
				return;

			x += Alignment - beginUnaligned;
			bytesCount -= Alignment - beginUnaligned;

			for (int i = 0; i < Alignment - beginUnaligned; i += 4)
				f (i);
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

			const __m128i pixel1msk = _mm_set_epi8 (0x80, 0x80, 0x80, 3,
													0x80, 0x80, 0x80, 2,
													0x80, 0x80, 0x80, 1,
													0x80, 0x80, 0x80, 0);
			const __m128i pixel2msk = _mm_set_epi8 (0x80, 0x80, 0x80, 7,
													0x80, 0x80, 0x80, 6,
													0x80, 0x80, 0x80, 5,
													0x80, 0x80, 0x80, 4);
			const __m128i pixel3msk = _mm_set_epi8 (0x80, 0x80, 0x80, 11,
													0x80, 0x80, 0x80, 10,
													0x80, 0x80, 0x80, 9,
													0x80, 0x80, 0x80, 8);
			const __m128i pixel4msk = _mm_set_epi8 (0x80, 0x80, 0x80, 15,
													0x80, 0x80, 0x80, 14,
													0x80, 0x80, 0x80, 13,
													0x80, 0x80, 0x80, 12);

			constexpr auto alignment = 16;

			for (int y = 0; y < height; ++y)
			{
				const uchar * const scanline = image.scanLine (y);

				int x = 0;
				int bytesCount = 0;

				HandleLoopBegin<alignment> (scanline, width, x, bytesCount,
						[&r, &g, &b, scanline] (int i)
						{
							auto color = *reinterpret_cast<const QRgb*> (&scanline [i]);
							r += qRed (color);
							g += qGreen (color);
							b += qBlue (color);
						});

				const auto endUnaligned = bytesCount % alignment;
				bytesCount -= endUnaligned;

				#pragma unroll(8)
				for (; x < bytesCount; x += alignment)
				{
					const __m128i fourPixels = _mm_load_si128 (reinterpret_cast<const __m128i*> (scanline + x));

					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel1msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel2msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel3msk));
					sum = _mm_add_epi32 (sum, _mm_shuffle_epi8 (fourPixels, pixel4msk));
				}

				for (int i = x; i < width * 4; i += 4)
				{
					auto color = *reinterpret_cast<const QRgb*> (&scanline [i]);
					r += qRed (color);
					g += qGreen (color);
					b += qBlue (color);
				}
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

			const __m256i ppair1mask = _mm256_set_epi8 (0x80, 0x80, 0x80, 7,
														0x80, 0x80, 0x80, 6,
														0x80, 0x80, 0x80, 5,
														0x80, 0x80, 0x80, 4,
														0x80, 0x80, 0x80, 3,
														0x80, 0x80, 0x80, 2,
														0x80, 0x80, 0x80, 1,
														0x80, 0x80, 0x80, 0);
			const __m256i ppair2mask = _mm256_set_epi8 (0x80, 0x80, 0x80, 15,
														0x80, 0x80, 0x80, 14,
														0x80, 0x80, 0x80, 13,
														0x80, 0x80, 0x80, 12,
														0x80, 0x80, 0x80, 11,
														0x80, 0x80, 0x80, 10,
														0x80, 0x80, 0x80, 9,
														0x80, 0x80, 0x80, 8);
			const __m256i ppair3mask = _mm256_set_epi8 (0x80, 0x80, 0x80, 23,
														0x80, 0x80, 0x80, 22,
														0x80, 0x80, 0x80, 21,
														0x80, 0x80, 0x80, 20,
														0x80, 0x80, 0x80, 19,
														0x80, 0x80, 0x80, 18,
														0x80, 0x80, 0x80, 17,
														0x80, 0x80, 0x80, 16);
			const __m256i ppair4mask = _mm256_set_epi8 (0x80, 0x80, 0x80, 31,
														0x80, 0x80, 0x80, 30,
														0x80, 0x80, 0x80, 29,
														0x80, 0x80, 0x80, 28,
														0x80, 0x80, 0x80, 27,
														0x80, 0x80, 0x80, 26,
														0x80, 0x80, 0x80, 25,
														0x80, 0x80, 0x80, 24);

			constexpr auto alignment = 32;

			for (int y = 0; y < height; ++y)
			{
				const uchar * const scanline = image.scanLine (y);

				const auto pos = scanline - static_cast<const uchar*> (nullptr);
				int x = 0;

				const auto beginUnaligned = pos % alignment;
				auto bytesCount = width * 4;
				if (beginUnaligned)
				{
					x += alignment - beginUnaligned;
					bytesCount -= alignment - beginUnaligned;

					for (int i = 0; i < alignment - beginUnaligned; i += 4)
					{
						auto color = *reinterpret_cast<const QRgb*> (&scanline [i]);
						r += qRed (color);
						g += qGreen (color);
						b += qBlue (color);
					}
				}

				const auto endUnaligned = bytesCount % alignment;
				bytesCount -= endUnaligned;

				for (; x < bytesCount; x += alignment)
				{
					const __m256i eightPixels = _mm256_load_si256 (reinterpret_cast<const __m256i*> (scanline + x));

					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair1mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair2mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair3mask));
					sum = _mm256_add_epi32 (sum, _mm256_shuffle_epi8 (eightPixels, ppair4mask));
				}

				for (int i = x / 4; i < width; ++i)
				{
					auto color = reinterpret_cast<const QRgb*> (image.scanLine (y)) [i];
					r += qRed (color);
					g += qGreen (color);
					b += qBlue (color);
				}
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
#ifdef Q_PROCESSOR_X86_64
			static const auto ptr = []
			{
				uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
				if (!__get_cpuid (7, &eax, &ebx, &ecx, &edx))
				{
					qWarning () << Q_FUNC_INFO
							<< "failed to get CPUID";
					return &GetGrayDefault;
				}

				if (ebx & (1 << 5))
				{
					qDebug () << Q_FUNC_INFO
							<< "detected AVX2 support";
					return &GetGrayAVX2;
				}

				if (!__get_cpuid (1, &eax, &ebx, &ecx, &edx))
				{
					qWarning () << Q_FUNC_INFO
							<< "failed to get CPUID";
					return &GetGrayDefault;
				}

				if (ecx & (1 << 19))
				{
					qDebug () << Q_FUNC_INFO
							<< "detected SSE 4.1 support";
					return &GetGraySSE4;
				}

				qDebug () << Q_FUNC_INFO
						<< "no particularly interesting SIMD IS, using default implementation";
				return GetGrayDefault;
			} ();

			return ptr (image);
#else
			return GetGrayDefault (image);
#endif
		}

		void Invert (QImage& image)
		{
			InvertDefault (image);
		}

		bool PrepareInverted (QImage& image, int threshold)
		{
			const auto height = image.height ();
			const auto width = image.width ();

			const auto sourceGraySum = GetGray (image) / (width * height * 32);
			const auto shouldInvert = sourceGraySum >= static_cast<uint64_t> (threshold);

			if (shouldInvert)
				Invert (image);

			return shouldInvert;
		}
	}

	void InvertEffect::draw (QPainter *painter)
	{
		QPoint offset;

		const auto& sourcePx = sourcePixmap (Qt::LogicalCoordinates, &offset, QGraphicsEffect::NoPad);
		auto image = sourcePx.toImage ();
		switch (image.format ())
		{
		case QImage::Format_ARGB32:
		case QImage::Format_ARGB32_Premultiplied:
			break;
		default:
			image = image.convertToFormat (QImage::Format_ARGB32);
			break;
		}
		image.detach ();

		if (PrepareInverted (image, Threshold_))
			painter->drawImage (offset, image);
		else
			painter->drawPixmap (offset, sourcePx);
	}
}
}
}
