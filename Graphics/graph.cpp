#include "graph.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <iostream>
#include <map>
#include <utility>
#include <wrl/client.h>

#include "Keyboard.h"

namespace graph
{
	template <class T>
	void SafeRelease(T& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = NULL;
		}
	}

	using Microsoft::WRL::ComPtr;
	ComPtr<IWICImagingFactory> g_pWICImagingFactory;
	ComPtr<IDWriteFactory> g_pDwriteFactory;
	ComPtr<ID2D1Factory> g_pD2DFactory;

	HRESULT CreateDeviceIndependentResources()
	{
		HRESULT hr = S_OK;
		if (g_pWICImagingFactory == nullptr)
		{
			hr = CoInitialize(NULL);

			if (SUCCEEDED(hr))
			{
				hr = CoCreateInstance(
				                      CLSID_WICImagingFactory2,
				                      nullptr,
				                      CLSCTX_INPROC_SERVER,
				                      __uuidof(IWICImagingFactory2),
				                      reinterpret_cast<void**>(g_pWICImagingFactory.GetAddressOf())
				                     );
			}

			if (FAILED(hr))
			{
				MessageBox(
				           GetForegroundWindow(),
				           TEXT("Create WICImage factory failed!"),
				           TEXT("Error"),
				           MB_OK);
				return hr;
			}
		}
		if (g_pD2DFactory == nullptr)
		{
			if (SUCCEEDED(hr))
			{
				hr = D2D1CreateFactory(
				                       D2D1_FACTORY_TYPE_SINGLE_THREADED,
				                       g_pD2DFactory.GetAddressOf());
			}

			if (FAILED(hr))
			{
				MessageBox(
				           GetForegroundWindow(),
				           TEXT("Create D2D factory failed!"),
				           TEXT("Error"),
				           MB_OK);
				return hr;
			}
		}
		if (g_pDwriteFactory == nullptr)
		{
			if (SUCCEEDED(hr))
			{
				hr = DWriteCreateFactory(
				                         DWRITE_FACTORY_TYPE_SHARED,
				                         __uuidof(IDWriteFactory),
				                         reinterpret_cast<IUnknown**>(g_pDwriteFactory.GetAddressOf()));
			}

			if (FAILED(hr))
			{
				MessageBox(
				           GetForegroundWindow(),
				           TEXT("Create DWrite factory failed!"),
				           TEXT("Error"),
				           MB_OK);
				return hr;
			}
		}
		return hr;
	}

	const UINT32 c_red_shift = 16;
	const UINT32 c_green_shift = 8;
	const UINT32 c_blue_shift = 0;

	const UINT32 c_red_mask = 0xff << c_red_shift;
	const UINT32 c_green_mask = 0xff << c_green_shift;
	const UINT32 c_blue_mask = 0xff << c_blue_shift;

	D2D1_SIZE_U Size2D2DU(const Size& size)
	{
		return D2D1::SizeU(
		                   static_cast<UINT32>(size.width),
		                   static_cast<UINT32>(size.height));
	}

	D2D1_SIZE_F Size2D2D(const Size& size)
	{
		return D2D1::SizeF(size.width, size.height);
	}

	D2D1_COLOR_F Color2D2D(const Color& color)
	{
		return D2D1::ColorF(color.red, color.green, color.blue, color.alpha);
	}

	D2D1_POINT_2F Point2D2D(const Point& point)
	{
		return D2D1::Point2F(point.x, point.y);
	}

	D2D1_RECT_F Rect2D2D(const Rect& rect)
	{
		return D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
	}

	D2D1_RECT_U Rect2D2DU(const Rect& rect)
	{
		return D2D1::RectU(
		                   static_cast<UINT32>(rect.left),
		                   static_cast<UINT32>(rect.top),
		                   static_cast<UINT32>(rect.right),
		                   static_cast<UINT32>(rect.bottom));
	}

	D2D1_ELLIPSE Ellipse2D2D(const Ellipse& ellipse)
	{
		return D2D1::Ellipse(
		                     Point2D2D(ellipse.center),
		                     ellipse.radius_x,
		                     ellipse.radius_y
		                    );
	}

	Ellipse Rect2Ellipse(const Rect& rect)
	{
		const float diameterX = rect.right - rect.left;
		const float diameterY = rect.bottom - rect.top;
		return Ellipse{
			{rect.left + diameterX / 2, rect.top + diameterY / 2},
			diameterX / 2,
			diameterY / 2
		};
	}

	Color::Color(const UINT8 r, const UINT8 g, const UINT8 b, const UINT8 a) :
		red(static_cast<float>(r) / 255.f),
		green(static_cast<float>(g) / 255.f),
		blue(static_cast<float>(b) / 255.f),
		alpha(static_cast<float>(a) / 255.f) {}

	Color::Color(const float r, const float g, const float b, const float a) :
		red(r), green(g), blue(b), alpha(a) {}

	Color::Color(COLORS rgb, const float a)
	{
		red = static_cast<float>((UINT(rgb) & c_red_mask) >> c_red_shift) / 255.f;
		green = static_cast<float>((UINT(rgb) & c_green_mask) >> c_green_shift) / 255.f;
		blue = static_cast<float>((UINT(rgb) & c_blue_mask) >> c_blue_shift) / 255.f;
		alpha = a;
	}

	bool Color::operator<(const Color& c) const
	{
		const UINT color =
				(static_cast<UINT>(red * 255.f) << c_red_shift) +
				(static_cast<UINT>(green * 255.f) << c_green_shift) +
				(static_cast<UINT>(blue * 255.f) << c_blue_shift);
		const UINT ccolor =
				(static_cast<UINT>(c.red * 255.f) << c_red_shift) +
				(static_cast<UINT>(c.green * 255.f) << c_green_shift) +
				(static_cast<UINT>(c.blue * 255.f) << c_blue_shift);
		return color < ccolor;
	}

	bool Color::operator==(const Color& c) const
	{
		return red == c.red && green == c.green && blue == c.blue && alpha == c.alpha;
	}

	Brush::~Brush()
	{
		if (is_owner)
		{
			SafeRelease(d2d_brush);
		}
	}

	Brush::Brush(Brush&& brush) noexcept
	{
		brush.is_owner = false;
		std::swap(d2d_brush, brush.d2d_brush);
	}

	void Brush::set_opacity(const float opacity)
	{
		d2d_brush->SetOpacity(opacity);
	}

	float Brush::get_opacity() const
	{
		return d2d_brush->GetOpacity();
	}

	Brush& Brush::operator=(Brush&& brush) noexcept
	{
		if (&brush != this)
		{
			brush.is_owner = false;
			std::swap(d2d_brush, brush.d2d_brush);
		}
		return *this;
	}

	D2DGraphics::D2DGraphics(const GraphSetting& setting) : setting(setting)
	{
		CreateDeviceIndependentResources();
		win_thread = std::thread([this]() { this->InitWindow(); });
	}

	D2DGraphics::~D2DGraphics()
	{
		if (win_thread.joinable())
		{
			win_thread.join();
		}
	}

	void D2DGraphics::clear(const Color color)
	{
		m_pRenderTarget->Clear(D2D1::ColorF(color.red, color.green, color.blue, color.alpha));
	}

	void D2DGraphics::begin_draw()
	{
		InitD2D();
		if (!has_began_draw)
		{
			DrawingLock();
			m_pRenderTarget->BeginDraw();
			has_began_draw = true;
		}
	}

	void D2DGraphics::end_draw()
	{
		if (has_began_draw)
		{
			m_pRenderTarget->EndDraw();
			has_began_draw = false;
			DrawingUnlock();
		}
	}

	DirectX::Keyboard::State D2DGraphics::get_keyboard_state()
	{
		return m_keyboard->GetState();
	}

	bool D2DGraphics::GetSolidColorBrush(const Color& color, ID2D1SolidColorBrush*& solidBrush)
	{
		HRESULT hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(
		                                                                 color.red,
		                                                                 color.green,
		                                                                 color.blue,
		                                                                 color.alpha
		                                                                ),
		                                                    &solidBrush);
		if (FAILED(hr))
		{
			MessageBox(
			           m_Hwnd,
			           TEXT("CreateBrush Fail"),
			           TEXT("Error"),
			           MB_OK);
			return false;
		}
		return true;
	}

	void D2DGraphics::draw_line(
		const Point from,
		const Point to,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		m_pRenderTarget->DrawLine(
		                          Point2D2D(from),
		                          Point2D2D(to),
		                          brush.d2d_brush,
		                          width,
		                          AutoGetStrokeStyle(style).Get());
	}

	void D2DGraphics::draw_triangle(
		const Point p1,
		const Point p2,
		const Point p3,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		draw_line(p1, p2, brush, width, style);
		draw_line(p2, p3, brush, width, style);
		draw_line(p3, p1, brush, width, style);
	}

	void D2DGraphics::draw_rect(
		const Rect rect,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		m_pRenderTarget->DrawRectangle(
		                               Rect2D2D(rect),
		                               brush.d2d_brush,
		                               width,
		                               AutoGetStrokeStyle(style).Get());
	}

	void D2DGraphics::draw_ellipse(
		const Ellipse ellipse,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		m_pRenderTarget->DrawEllipse(
		                             Ellipse2D2D(ellipse),
		                             brush.d2d_brush,
		                             width,
		                             AutoGetStrokeStyle(style).Get());
	}

	void D2DGraphics::draw_ellipse(
		const Rect rect,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		draw_ellipse(Rect2Ellipse(rect), brush, width, style);
	}

	void D2DGraphics::draw_poly(
		const Point* points,
		const size_t size,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		for (size_t i = 0; i < size - 1; i++)
		{
			draw_line(points[i], points[i + 1], brush, width, style);
		}
		draw_line(points[size - 1], points[0], brush, width, style);
	}

	void D2DGraphics::draw_poly(
		const std::vector<Point>& points,
		const Brush& brush,
		const float width,
		const STROKE_STYLE style)
	{
		if (brush.d2d_brush == nullptr) { return; }
		draw_poly(points.data(), points.size(), brush, width, style);
	}

	HRESULT LoadBitmapFromFile(
		ID2D1RenderTarget* pRenderTarget,
		IWICImagingFactory* pIWICFactory,
		PCWSTR uri,
		UINT destinationWidth,
		UINT destinationHeight,
		ID2D1Bitmap** ppBitmap
	)
	{
		IWICBitmapDecoder* pDecoder = NULL;
		IWICBitmapFrameDecode* pSource = NULL;
		IWICStream* pStream = NULL;
		IWICFormatConverter* pConverter = NULL;
		IWICBitmapScaler* pScaler = NULL;

		HRESULT hr = pIWICFactory->CreateDecoderFromFilename(
		                                                     uri,
		                                                     NULL,
		                                                     GENERIC_READ,
		                                                     WICDecodeMetadataCacheOnLoad,
		                                                     &pDecoder
		                                                    );

		if (SUCCEEDED(hr))
		{
			// Create the initial frame.
			hr = pDecoder->GetFrame(0, &pSource);
		}

		if (SUCCEEDED(hr))
		{
			// Convert the image format to 32bppPBGRA
			// (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
			hr = pIWICFactory->CreateFormatConverter(&pConverter);
		}


		if (SUCCEEDED(hr))
		{
			hr = pConverter->Initialize(
			                            pSource,
			                            GUID_WICPixelFormat32bppPBGRA,
			                            WICBitmapDitherTypeNone,
			                            NULL,
			                            0.f,
			                            WICBitmapPaletteTypeMedianCut
			                           );
		}

		if (SUCCEEDED(hr))
		{
			// Create a Direct2D bitmap from the WIC bitmap.
			hr = pRenderTarget->CreateBitmapFromWicBitmap(
			                                              pConverter,
			                                              NULL,
			                                              ppBitmap
			                                             );
		}

		SafeRelease(pDecoder);
		SafeRelease(pSource);
		SafeRelease(pStream);
		SafeRelease(pConverter);
		SafeRelease(pScaler);

		return hr;
	}

	Bitmap D2DGraphics::load_image_from_file(const std::wstring& filePath)
	{
		Bitmap res;
		LoadBitmapFromFile(m_pRenderTarget.Get(), g_pWICImagingFactory.Get(), filePath.c_str(), 0, 0, &res.d2d_bitmap);
		return res;
	}

	Bitmap D2DGraphics::create_image_from_memory(const Size size, const void* srcData, const UINT pitch)
	{
		Bitmap res;
		HRESULT hr = m_pRenderTarget->CreateBitmap(
		                                           Size2D2DU(size),
		                                           D2D1::BitmapProperties(
		                                                                  D2D1::PixelFormat(
		                                                                                    DXGI_FORMAT_B8G8R8A8_UNORM,
		                                                                                    D2D1_ALPHA_MODE_PREMULTIPLIED)),
		                                           &res.d2d_bitmap);
		if (SUCCEEDED(hr))
		{
			hr = res.d2d_bitmap->CopyFromMemory(nullptr, srcData, pitch);
		}
		return res;
	}

	Bitmap D2DGraphics::create_image_from_memory(const Size size, const ColorBGRA8bit* srcData)
	{
		return create_image_from_memory(size, srcData, 4 * static_cast<UINT>(size.width));
	}

	void D2DGraphics::draw_image(const Rect rect, const Bitmap& bitmap)
	{
		if (bitmap.d2d_bitmap == nullptr) { return; }
		m_pRenderTarget->DrawBitmap(bitmap.d2d_bitmap, Rect2D2D(rect));
	}

	Font D2DGraphics::create_font(
		const std::wstring& fontName,
		const float fontSize,
		FONT_WEIGHT fontWeight,
		FONT_STYLE fontStyle,
		FONT_STRETCH fontStretch)
	{
		Font res;
		g_pDwriteFactory->CreateTextFormat(
		                                   fontName.c_str(),
		                                   nullptr,
		                                   DWRITE_FONT_WEIGHT(fontWeight),
		                                   DWRITE_FONT_STYLE(fontStyle),
		                                   DWRITE_FONT_STRETCH(fontStretch),
		                                   fontSize,
		                                   L"",
		                                   &res.d2d_font);
		res.d2d_font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		return res;
	}

	void D2DGraphics::draw_text(
		const std::wstring& text,
		const Rect rect,
		const Font& font,
		const Brush& brush,
		TEXT_ALIGN_HORIZONTAL alignHorizontal,
		TEXT_ALIGN_VERTICAL alignVertical)
	{
		if (font.d2d_font == nullptr || brush.d2d_brush == nullptr) { return; }
		IDWriteTextLayout* layout;
		HRESULT hr = g_pDwriteFactory->CreateTextLayout(
		                                                text.c_str(),
		                                                text.size(),
		                                                font.d2d_font,
		                                                rect.right - rect.left,
		                                                rect.bottom - rect.top,
		                                                &layout
		                                               );
		if (FAILED(hr)) { return; }
		layout->SetTextAlignment(static_cast<DWRITE_TEXT_ALIGNMENT>(alignHorizontal));
		layout->SetParagraphAlignment(static_cast<DWRITE_PARAGRAPH_ALIGNMENT>(alignVertical));
		m_pRenderTarget->DrawTextLayout(
		                                D2D1::Point2F(rect.left, rect.top),
		                                layout,
		                                brush.d2d_brush);
	}

	void D2DGraphics::fill_triangle(const Point p1, const Point p2, const Point p3, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		ID2D1PathGeometry* geometry = NULL;
		HRESULT hr = g_pD2DFactory->CreatePathGeometry(&geometry);
		if (FAILED(hr))
		{
			MessageBox(m_Hwnd, TEXT("Create Geometry Fail"), TEXT("Error"), MB_OK);
			return;
		}
		ID2D1GeometrySink* pSink = NULL;
		hr = geometry->Open(&pSink);
		if (FAILED(hr))
		{
			MessageBox(m_Hwnd, TEXT("Open Geometry Fail"), TEXT("Error"), MB_OK);
			return;
		}
		pSink->BeginFigure(Point2D2D(p1), D2D1_FIGURE_BEGIN_FILLED);
		D2D1_POINT_2F points[] = {
			Point2D2D(p1), Point2D2D(p2), Point2D2D(p3)
		};
		pSink->AddLines(points, 3);
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
		pSink->Close();
		SafeRelease(pSink);
		m_pRenderTarget->FillGeometry(geometry, brush.d2d_brush);
		SafeRelease(geometry);
	}

	void D2DGraphics::fill_rect(const Rect rect, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		m_pRenderTarget->FillRectangle(
		                               Rect2D2D(rect),
		                               brush.d2d_brush);
	}

	void D2DGraphics::fill_ellipse(const Ellipse ellipse, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		m_pRenderTarget->FillEllipse(
		                             Ellipse2D2D(ellipse),
		                             brush.d2d_brush
		                            );
	}

	void D2DGraphics::fill_ellipse(const Rect rect, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		fill_ellipse(Rect2Ellipse(rect), brush);
	}

	void D2DGraphics::fill_poly(const Point* points, const size_t size, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		ID2D1PathGeometry* geometry = NULL;
		HRESULT hr = g_pD2DFactory->CreatePathGeometry(&geometry);
		if (FAILED(hr))
		{
			MessageBox(
			           m_Hwnd,
			           TEXT("Create Geometry Fail"),
			           TEXT("Error"),
			           MB_OK);
			return;
		}
		ID2D1GeometrySink* pSink = NULL;
		hr = geometry->Open(&pSink);
		if (FAILED(hr))
		{
			MessageBox(
			           m_Hwnd,
			           TEXT("Open Geometry Fail"),
			           TEXT("Error"),
			           MB_OK);
			return;
		}
		pSink->BeginFigure(Point2D2D(points[0]), D2D1_FIGURE_BEGIN_FILLED);
		std::vector<D2D1_POINT_2F> d2dPoints(size);
		for (size_t i = 0; i < size; i++)
		{
			d2dPoints[i] = Point2D2D(points[i]);
		}
		pSink->AddLines(d2dPoints.data(), static_cast<UINT32>(size));
		pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
		pSink->Close();
		SafeRelease(pSink);
		m_pRenderTarget->FillGeometry(geometry, brush.d2d_brush);
		SafeRelease(geometry);
	}

	void D2DGraphics::fill_poly(const std::vector<Point>& points, const Brush& brush)
	{
		if (brush.d2d_brush == nullptr) { return; }
		fill_poly(points.data(), points.size(), brush);
	}

	void D2DGraphics::set_pixel(const float x, const float y, const Color color)
	{
		draw_line(Point{x, y}, Point{x + 1, y}, SolidBrush(color));
	}

	void D2DGraphics::set_pixel(const Point point, const Color color)
	{
		set_pixel(point.x, point.y, color);
	}

	void D2DGraphics::InitializeDPIScale(const HWND hwnd)
	{
#ifdef  NTDDI_WIN10
		const float dpi = static_cast<float>(GetDpiForWindow(hwnd));
		DPI_scaleX = dpi / 96.0f;
		DPI_scaleY = dpi / 96.0f;
#else
		float dpix = 96.f, dpiy = 96.f;
		if (m_pRenderTarget)
		{
			m_pRenderTarget->GetDpi(&dpix, &dpiy);
		}
		//const float dpi = static_cast<float>(GetDpiForWindow(hwnd));
		DPI_scaleX = dpix / 96.0f;
		DPI_scaleY = dpiy / 96.0f;
#endif
	}

	void D2DGraphics::DrawingLock()
	{
		while (in_drawing.test_and_set(std::memory_order_acquire));
	}

	void D2DGraphics::DrawingUnlock()
	{
		in_drawing.clear(std::memory_order_release);
	}

	void D2DGraphics::PausingLock()
	{
		while (can_pause.test_and_set(std::memory_order_acquire));
	}

	void D2DGraphics::PausingUnlock()
	{
		can_pause.clear(std::memory_order_release);
	}

	ULONGLONG D2DGraphics::get_frame_counter()
	{
		return frame_counter;
	}

	void D2DGraphics::reset_frame_counter()
	{
		frame_counter = 0;
	}

	Point D2DGraphics::get_relative_pos()
	{
		POINT pos;
		GetCursorPos(&pos);
		ScreenToClient(m_Hwnd, &pos);
		const Point dipPos = PixelsToDips(pos.x, pos.y);
		return dipPos;
	}

	Size D2DGraphics::get_dip_size()
	{
		const auto size = m_pRenderTarget->GetSize();
		return Size{static_cast<float>(size.width), static_cast<float>(size.height)};
	}

	Size D2DGraphics::get_pixel_size()
	{
		const auto size = m_pRenderTarget->GetPixelSize();
		return Size{static_cast<float>(size.width), static_cast<float>(size.height)};
	}

	std::wstring D2DGraphics::get_caption()
	{
		const size_t len = GetWindowTextLength(m_Hwnd);
		auto* buf = new wchar_t[len + 5];
		GetWindowText(m_Hwnd, buf, static_cast<int>(len + 5));
		setting.window_caption = buf;
		delete[] buf;
		return setting.window_caption;
	}

	void D2DGraphics::set_caption(const std::wstring& caption)
	{
		setting.window_caption = caption;
		SetWindowText(m_Hwnd, caption.c_str());
	}

	void D2DGraphics::rotate_view(float angle, const Point center)
	{
		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Rotation(
		                                                         angle / TWO_PI * 360.f,
		                                                         Point2D2D(center)));
	}

	void D2DGraphics::reset_view()
	{
		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	void D2DGraphics::show_scene(const int index)
	{
		current_scene = setting.Scenes[index];
		static std::vector<bool> inited(setting.Scenes.size(), false);
		if (setting.Init_option == GraphSetting::INIT_OPTION::INIT_ONCE_BEFORE_USING && !inited[index])
		{
			current_scene->init(this);
			inited[index] = true;
		}
		else if (setting.Init_option == GraphSetting::INIT_OPTION::ALWAYS_INIT_BEFORE_USING)
		{
			current_scene->init(this);
		}
	}

	void D2DGraphics::close()
	{
		SendMessage(m_Hwnd,WM_DESTROY, 0, 0);
	}

	void D2DGraphics::reset_size(const UINT width, const UINT height)
	{
		RECT winRect{};
		GetWindowRect(m_Hwnd, &winRect);


#ifdef NTDDI_WIN10
		const UINT dpi = GetDpiForSystem();
		RECT newRect;
		newRect.left = newRect.top = 0;
		newRect.right = static_cast<LONG>(setting.width * dpi / 96);
		newRect.bottom = static_cast<LONG>(setting.height * dpi / 96);
#else
		float dpix, dpiy;
		m_pRenderTarget->GetDpi(&dpix, &dpiy);
		RECT newRect;
		newRect.left = newRect.top = 0;
		newRect.right = static_cast<LONG>(setting.width * dpix / 96);
		newRect.bottom = static_cast<LONG>(setting.height * dpiy / 96);
#endif


		DWORD wStyle = WS_OVERLAPPEDWINDOW;
		if (!setting.window_can_resize)
		{
			wStyle = wStyle ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
		}

		AdjustWindowRect(&newRect, wStyle, FALSE);
		MoveWindow(m_Hwnd, winRect.left, winRect.top, newRect.right - newRect.left, newRect.bottom - newRect.top, TRUE);
	}

	void D2DGraphics::pause()
	{
		PausingLock();
	}

	void D2DGraphics::resume()
	{
		PausingUnlock();
	}

	LONGLONG get_time()
	{
		LARGE_INTEGER tick;
		QueryPerformanceCounter(&tick);
		return tick.QuadPart;
	}

	//Resize will waiting when renderTarget is in drawing
	bool D2DGraphics::Resize(const unsigned width, const unsigned height)
	{
		if (m_pRenderTarget != nullptr)
		{
			DrawingLock();
			const HRESULT hr = m_pRenderTarget->Resize(D2D1::SizeU(width, height));
			DrawingUnlock();
			return SUCCEEDED(hr);
		}
		return true;
	}

	ComPtr<ID2D1StrokeStyle> D2DGraphics::GetDashStyle()
	{
		if (stroke_style_dash == nullptr)
		{
			HRESULT hr = g_pD2DFactory->CreateStrokeStyle(
			                                              D2D1::StrokeStyleProperties(
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_LINE_JOIN_MITER,
			                                                                          10.0f,
			                                                                          D2D1_DASH_STYLE_DASH,
			                                                                          0.0f
			                                                                         ),
			                                              NULL,
			                                              0,
			                                              stroke_style_dash.GetAddressOf()
			                                             );
			if (FAILED(hr))
			{
				MessageBox(
				           m_Hwnd,
				           TEXT("CreateStrokeStyle Dash Fail"),
				           TEXT("Error"),
				           MB_OK);
				return NULL;
			}
		}
		return stroke_style_dash;
	}

	ComPtr<ID2D1StrokeStyle> D2DGraphics::GetDashDotStyle()
	{
		if (!stroke_style_dashdot)
		{
			HRESULT hr = g_pD2DFactory->CreateStrokeStyle(
			                                              D2D1::StrokeStyleProperties(
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_LINE_JOIN_MITER,
			                                                                          10.0f,
			                                                                          D2D1_DASH_STYLE_DASH_DOT,
			                                                                          0.0f
			                                                                         ),
			                                              NULL,
			                                              0,
			                                              stroke_style_dashdot.GetAddressOf()
			                                             );
			if (FAILED(hr))
			{
				MessageBox(
				           m_Hwnd,
				           TEXT("CreateStrokeStyle DashDot Fail"),
				           TEXT("Error"),
				           MB_OK);
				return NULL;
			}
		}
		return stroke_style_dashdot;
	}

	ComPtr<ID2D1StrokeStyle> D2DGraphics::GetDashDotDotStyle()
	{
		if (stroke_style_dashdotdot == nullptr)
		{
			HRESULT hr = g_pD2DFactory->CreateStrokeStyle(
			                                              D2D1::StrokeStyleProperties(
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_LINE_JOIN_MITER,
			                                                                          10.0f,
			                                                                          D2D1_DASH_STYLE_DASH_DOT_DOT,
			                                                                          0.0f
			                                                                         ),
			                                              NULL,
			                                              0,
			                                              stroke_style_dashdotdot.GetAddressOf()
			                                             );
			if (FAILED(hr))
			{
				MessageBox(
				           m_Hwnd,
				           TEXT("CreateStrokeStyle DashDotDot Fail"),
				           TEXT("Error"),
				           MB_OK);
				return NULL;
			}
		}
		return stroke_style_dashdotdot;
	}

	ComPtr<ID2D1StrokeStyle> D2DGraphics::GetDotStyle()
	{
		if (stroke_style_dot == nullptr)
		{
			HRESULT hr = g_pD2DFactory->CreateStrokeStyle(
			                                              D2D1::StrokeStyleProperties(
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_CAP_STYLE_ROUND,
			                                                                          D2D1_LINE_JOIN_MITER,
			                                                                          10.0f,
			                                                                          D2D1_DASH_STYLE_DOT,
			                                                                          0.0f
			                                                                         ),
			                                              NULL,
			                                              0,
			                                              stroke_style_dot.GetAddressOf()
			                                             );
			if (FAILED(hr))
			{
				MessageBox(m_Hwnd, TEXT("CreateStrokeStyle Fail"), TEXT("Error"), MB_OK);
				return NULL;
			}
		}
		return stroke_style_dot;
	}

	ComPtr<ID2D1StrokeStyle> D2DGraphics::AutoGetStrokeStyle(const STROKE_STYLE style)
	{
		switch (style)
		{
			case STROKE_STYLE::Dash:
				return GetDashStyle();
			case STROKE_STYLE::DashDot:
				return GetDashDotStyle();
			case STROKE_STYLE::DashDotDot:
				return GetDashDotDotStyle();
			case STROKE_STYLE::Dot:
				return GetDotStyle();
			case STROKE_STYLE::Soild:
				return 0;
		}
		return nullptr;
	}

	DWORD D2DGraphics::InitWindow()
	{
		HINSTANCE hInstance = ::GetModuleHandle(0);

		WNDCLASSEX winClass;

		winClass.lpszClassName = TEXT("d2dgraph");
		winClass.cbSize = sizeof(WNDCLASSEX);
		winClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		winClass.lpfnWndProc = WndProcImpl;
		winClass.hInstance = hInstance;
		winClass.hIcon = NULL;
		winClass.hIconSm = NULL;
		winClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		winClass.hbrBackground = NULL;
		winClass.lpszMenuName = NULL;
		winClass.cbClsExtra = 0;
		winClass.cbWndExtra = 0;

		if (!RegisterClassEx(&winClass))
		{
			MessageBox(
			           NULL,
			           TEXT("This program requires Windows NT!"),
			           TEXT("error"),
			           MB_ICONERROR);
			return 0;
		}

		DWORD wStyle = WS_OVERLAPPEDWINDOW;
		if (!setting.window_can_resize)
		{
			wStyle = wStyle ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
		}

#ifdef NTDDI_WIN10
		const UINT dpi = GetDpiForSystem();

		RECT winRect;
		winRect.left = winRect.top = 0;
		winRect.right = static_cast<LONG>(setting.width * dpi / 96);
		winRect.bottom = static_cast<LONG>(setting.height * dpi / 96);
#else
		RECT winRect;
		winRect.left = winRect.top = 0;
		winRect.right = static_cast<LONG>(setting.width);
		winRect.bottom = static_cast<LONG>(setting.height);
#endif
		AdjustWindowRect(&winRect, wStyle, false);

		m_Hwnd = CreateWindowEx(
		                        NULL,
		                        TEXT("d2dgraph"),
		                        setting.window_caption.c_str(),
		                        wStyle,
		                        CW_USEDEFAULT,
		                        CW_USEDEFAULT,
		                        winRect.right - winRect.left,
		                        winRect.bottom - winRect.top,
		                        NULL,
		                        NULL,
		                        hInstance,
		                        this
		                       );
		UpdateWindow(m_Hwnd);
		ShowWindow(m_Hwnd, SW_SHOW);
		InitD2D();
		InitScene();
		if (!setting.Scenes.empty() && 0 <= setting.first_show_scene && setting.first_show_scene < setting.Scenes.size()
		)
		{
			show_scene(setting.first_show_scene);
		}
		MSG msg;
		ZeroMemory(&msg, sizeof(msg));
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				if (current_scene && m_pRenderTarget->CheckWindowState() != D2D1_WINDOW_STATE_OCCLUDED)
				{
					current_scene->update(this);
					begin_draw();
					current_scene->render(this);
					frame_counter++;
					end_draw();
				}
			}
		}

		return 0;
	}

	LRESULT CALLBACK D2DGraphics::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
			case WM_CREATE:
				break;

			case WM_PAINT:
				{
					PAINTSTRUCT ps;
					BeginPaint(hwnd, &ps);
					EndPaint(hwnd, &ps);
					break;
				}

			case WM_ACTIVATEAPP:
				DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
				break;

			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
				DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
				break;
			
			case WM_SIZE:
				Resize(LOWORD(lParam), HIWORD(lParam));
				break;

			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;

			default: ;
		}
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	LRESULT D2DGraphics::WndProcImpl(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		D2DGraphics* pThis;

		if (message == WM_NCCREATE)
		{
			pThis = static_cast<D2DGraphics*>(
				reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

			SetLastError(0);
			if (!SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis)))
			{
				if (GetLastError() != 0)
					return FALSE;
			}
		}
		else
		{
			pThis = reinterpret_cast<D2DGraphics*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		if (pThis)
		{
			return pThis->WndProc(hwnd, message, wParam, lParam);
		}

		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	bool D2DGraphics::InitD2D()
	{
		if (!m_pRenderTarget)
		{
			// Obtain the size of the drawing area
			RECT rc;
			GetClientRect(m_Hwnd, &rc);
			// Create a Direct2D render target
			HRESULT hr = g_pD2DFactory->CreateHwndRenderTarget(
			                                                   D2D1::RenderTargetProperties(),
			                                                   D2D1::HwndRenderTargetProperties(
			                                                                                    m_Hwnd,
			                                                                                    D2D1::SizeU(rc.right -
			                                                                                                rc.left,
			                                                                                                rc.bottom -
			                                                                                                rc.top)
			                                                                                   ),
			                                                   m_pRenderTarget.GetAddressOf()
			                                                  );

			if (FAILED(hr))
			{
				MessageBox(m_Hwnd, TEXT("Create render target failed!"), TEXT("Error"), 0);
				return false;
			}

			InitializeDPIScale(m_Hwnd);

			//m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		}
		return true;
	}

	void D2DGraphics::InitScene()
	{
		switch (setting.Init_option)
		{
			case GraphSetting::INIT_OPTION::INIT_ALL_SCENE_BEFORE_RUN:
				{
					for (auto* s : setting.Scenes)
					{
						s->init(this);
					}
				}
			case GraphSetting::INIT_OPTION::INIT_ONCE_BEFORE_USING:
			case GraphSetting::INIT_OPTION::NEVER_INIT:
			default: break;
		}
	}

	SolidBrush::SolidBrush(const Color color) : Brush(), color(color) {}

	SolidBrush::SolidBrush(SolidBrush&& preBrush) noexcept
	{
		preBrush.is_owner = false;
		std::swap(d2d_brush, preBrush.d2d_brush);
		std::swap(color, preBrush.color);
	}

	SolidBrush& SolidBrush::operator=(SolidBrush&& preBrush) noexcept
	{
		if (&preBrush != this)
		{
			preBrush.is_owner = false;
			std::swap(d2d_brush, preBrush.d2d_brush);
			std::swap(color, preBrush.color);
		}
		return *this;
	}

	Color SolidBrush::get_color() const
	{
		return color;
	}

	Bitmap::Bitmap(const std::wstring& filePath, D2DGraphics& graphics)
	{
		LoadBitmapFromFile(
		                   graphics.m_pRenderTarget.Get(),
		                   g_pWICImagingFactory.Get(),
		                   filePath.c_str(),
		                   0,
		                   0,
		                   &d2d_bitmap);
	}

	Bitmap::~Bitmap()
	{
		if (is_owner)
		{
			SafeRelease(d2d_bitmap);
		}
	}

	Bitmap::Bitmap(Bitmap&& preBitmap) noexcept
	{
		preBitmap.is_owner = false;
		std::swap(d2d_bitmap, preBitmap.d2d_bitmap);
	}

	Bitmap& Bitmap::operator=(Bitmap&& preBitmap) noexcept
	{
		if (&preBitmap != this)
		{
			preBitmap.is_owner = false;
			std::swap(d2d_bitmap, preBitmap.d2d_bitmap);
		}
		return *this;
	}

	Size Bitmap::get_size() const
	{
		const D2D1_SIZE_F size = d2d_bitmap->GetSize();
		return Size{size.width, size.height};
	}

	Font::~Font()
	{
		if (is_owner)
		{
			SafeRelease(d2d_font);
		}
	}

	Font::Font(Font&& preFont) noexcept
	{
		preFont.is_owner = false;
		std::swap(d2d_font, preFont.d2d_font);
	}

	Font& Font::operator=(Font&& preFont) noexcept
	{
		if (&preFont != this)
		{
			preFont.is_owner = false;
			std::swap(d2d_font, preFont.d2d_font);
		}
		return *this;
	}

	std::wstring Font::get_name() const
	{
		const size_t len = d2d_font->GetFontFamilyNameLength();
		auto* buf = new WCHAR[len + 5];
		d2d_font->GetFontFamilyName(buf, static_cast<UINT32>(len));
		std::wstring name(buf);
		delete[] buf;
		return name;
	}

	SolidBrush D2DGraphics::create_solidbrush(const Color color)
	{
		SolidBrush solidBrush(color);
		InitD2D();
		m_pRenderTarget->CreateSolidColorBrush(
		                                       Color2D2D(color),
		                                       reinterpret_cast<ID2D1SolidColorBrush**>(&solidBrush.d2d_brush));
		return solidBrush;
	}

	const SolidBrush& D2DGraphics::get_solidbrush(const Color color)
	{
		if (brushes.find(color) == brushes.end())
		{
			brushes[color] = std::make_unique<SolidBrush>(create_solidbrush(color));
		}
		return *brushes[color];
	}
}
