/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/Screenshot.hh>
#include <imagine/data-type/image/sys.hh>
#include <imagine/pixmap/Pixmap.hh>
#include <imagine/io/sys.hh>
#include <imagine/fs/sys.hh>

#ifdef CONFIG_DATA_TYPE_IMAGE_QUARTZ2D

bool writeScreenshot(const IG::Pixmap &vidPix, const char *fname)
{
	auto screen = vidPix.data;
	auto tempImgBuff = (char*)mem_alloc(vidPix.x * vidPix.y * 3);
	IG::Pixmap tempPix(PixelFormatRGB888);
	tempPix.init(tempImgBuff, vidPix.x, vidPix.y);
	for(uint y = 0; y < vidPix.y; y++, screen += vidPix.pitch, tempImgBuff += tempPix.pitch)
	{
		auto rowpix = tempImgBuff;
		for(uint x = 0; x < vidPix.x; x++)
		{
			// assumes RGB565
			uint16 pixVal = *(uint16 *)(screen+2*x);
			uint32 r = pixVal >> 11, g = (pixVal >> 5) & 0x3f, b = pixVal & 0x1f;
			r *= 8; g *= 4; b *= 8;
			*(rowpix++) = r;
			*(rowpix++) = g;
			*(rowpix++) = b;
		}
	}
	Quartz2dImage::writeImage(tempPix, fname);
	mem_free(tempPix.data);
	logMsg("%s saved.", fname);
	return 1;
}

#elif defined CONFIG_DATA_TYPE_IMAGE_ANDROID

// TODO: make png writer module in imagine
namespace Base
{

JNIEnv* jEnv(); // JNIEnv of main event thread
extern jclass jBaseActivityCls;
extern jobject jBaseActivity;

}

bool writeScreenshot(const IG::Pixmap &vidPix, const char *fname)
{
	static JavaInstMethod<jobject> jMakeBitmap;
	static JavaInstMethod<jobject> jWritePNG;
	using namespace Base;
	auto env = jEnv();
	if(!jMakeBitmap)
	{
		jMakeBitmap.setup(env, jBaseActivityCls, "makeBitmap", "(III)Landroid/graphics/Bitmap;");
		jWritePNG.setup(env, jBaseActivityCls, "writePNG", "(Landroid/graphics/Bitmap;Ljava/lang/String;)Z");
	}
	auto bitmap = jMakeBitmap(env, jBaseActivity, vidPix.x, vidPix.y, ANDROID_BITMAP_FORMAT_RGB_565);
	if(!bitmap)
	{
		logErr("error allocating bitmap");
		return false;
	}
	AndroidBitmapInfo info;
	AndroidBitmap_getInfo(env, bitmap, &info);
	logMsg("%d %d %d", info.width, info.height, info.stride);
	assert(info.format == ANDROID_BITMAP_FORMAT_RGB_565);
	void *buffer;
	AndroidBitmap_lockPixels(env, bitmap, &buffer);
	Pixmap dest(PixelFormatRGB565);
	dest.init2((char*)buffer, info.width, info.height, info.stride);
	vidPix.copy(0, 0, 0, 0, dest, 0, 0);
	AndroidBitmap_unlockPixels(env, bitmap);
	auto nameJStr = env->NewStringUTF(fname);
	auto writeOK = jWritePNG(env, jBaseActivity, bitmap, nameJStr);
	env->DeleteLocalRef(nameJStr);
	env->DeleteLocalRef(bitmap);
	if(!writeOK)
	{
		logErr("error writing PNG");
		return false;
	}
	logMsg("%s saved.", fname);
	return true;
}

#else

static void png_ioWriter(png_structp pngPtr, png_bytep data, png_size_t length)
{
	Io *stream = (Io*)png_get_io_ptr(pngPtr);

	if(stream->fwrite(data, length, 1) != 1)
	{
		logErr("error writing png file");
		//png_error(pngPtr, "Write Error");
	}
}

static void png_ioFlush(png_structp pngPtr)
{
	logMsg("called png_ioFlush");
}

bool writeScreenshot(const IG::Pixmap &vidPix, const char *fname)
{
	auto fp = IOFile(IoSys::create(fname));
	if(!fp)
	{
		return false;
	}

	png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!pngPtr)
	{
		fp.close();
		FsSys::remove(fname);
		return false;
	}
	png_infop infoPtr = png_create_info_struct(pngPtr);
	if(!infoPtr)
	{
		png_destroy_write_struct(&pngPtr, (png_infopp)NULL);
		fp.close();
		FsSys::remove(fname);
		return false;
	}

	if(setjmp(png_jmpbuf(pngPtr)))
	{
		png_destroy_write_struct(&pngPtr, &infoPtr);
		fp.close();
		FsSys::remove(fname);
		return false;
	}

	uint imgwidth = vidPix.x;
	uint imgheight = vidPix.y;

	png_set_write_fn(pngPtr, fp.io(), png_ioWriter, png_ioFlush);
	png_set_IHDR(pngPtr, infoPtr, imgwidth, imgheight, 8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info(pngPtr, infoPtr);

	//png_set_packing(pngPtr);

	png_byte *rowPtr= (png_byte*)mem_alloc(png_get_rowbytes(pngPtr, infoPtr));
	auto screen = vidPix.data;
	for(uint y=0; y < vidPix.y; y++, screen+=vidPix.pitch)
	{
		png_byte *rowpix = rowPtr;
		for(uint x=0; x < vidPix.x; x++)
		{
			// assumes RGB565
			uint16 pixVal = *(uint16 *)(screen+2*x);
			uint32 r = pixVal >> 11, g = (pixVal >> 5) & 0x3f, b = pixVal & 0x1f;
			r *= 8; g *= 4; b *= 8;
			*(rowpix++) = r;
			*(rowpix++) = g;
			*(rowpix++) = b;
			if(imgwidth!=vidPix.x)
			{
				*(rowpix++) = r;
				*(rowpix++) = g;
				*(rowpix++) = b;
			}
		}
		png_write_row(pngPtr, rowPtr);
		if(imgheight!=vidPix.y)
			png_write_row(pngPtr, rowPtr);
	}

	mem_free(rowPtr);

	png_write_end(pngPtr, infoPtr);
	png_destroy_write_struct(&pngPtr, &infoPtr);

	logMsg("%s saved.", fname);
	return true;
}

#endif
