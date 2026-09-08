#pragma once
class CGraphics
{
	static CGraphics * singleInstance;
	CGraphics() {}
public:
	static CGraphics * Get();
	// Screen size in pixels: the game viewport when its offset is known, else
	// ImGui's display size, else the window's client area. Returns false when
	// only a default could be provided.
	bool ScreenSize(float & width, float & height);
	// Normalised screen coordinates (0..1) of a world position; false when
	// behind the camera. Without the viewport structure the game's own
	// projection native is used, which is only valid from the script thread.
	bool WorldToScreen(CVector3 pos, CVector3 & out);
	void Draw3DText(std::string text, float x, float y, float z, color_t color);
	void Draw3DProgressBar(color_t bgColor, color_t frontColor, float width, float height, float worldX, float worldY, float worldZ, float value);
	~CGraphics();
};
