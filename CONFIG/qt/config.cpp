#include "config.h"

#include "MainDlg.h"
#include "detectdx5.h"

#include <mxdirectx/legodxinfo.h>
#include <mxdirectx/mxdirect3d.h>
#ifdef MINIWIN
#include "miniwin/direct.h"
#include "miniwin/process.h"
#else
#include <direct.h>  // _chdir
#include <process.h> // _spawnl
#endif

#include "inifile.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <SDL3/SDL.h>
#include <iniparser.h>

DECOMP_SIZE_ASSERT(CWinApp, 0xc4)
DECOMP_SIZE_ASSERT(CConfigApp, 0x108)

DECOMP_STATIC_ASSERT(offsetof(CConfigApp, m_display_bit_depth) == 0xd0)

// FUNCTION: CONFIG 0x00402c40
CConfigApp::CConfigApp()
{
	char* prefPath = SDL_GetPrefPath("isledecomp", "isle");
	char* iniConfig;
	if (prefPath) {
		m_iniPath = std::string{prefPath} + "isle.ini";
	}
	else {
		m_iniPath = "isle.ini";
	}
	SDL_free(prefPath);
}

// FUNCTION: CONFIG 0x00402dc0
bool CConfigApp::InitInstance()
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		QString err = QString{"SDL failed to initialize ("} + SDL_GetError() + ")";
		QMessageBox::warning(nullptr, "SDL initialization error", err);
		return false;
	}
	if (!DetectDirectX5()) {
		QMessageBox::warning(
			nullptr,
			"Missing DirectX",
			"\"LEGO\xc2\xae Island\" is not detecting DirectX 5 or later.  Please quit all other applications and try "
			"again."
		);
		return false;
	}
	m_device_enumerator = new LegoDeviceEnumerate;
	SDL_Window* window = SDL_CreateWindow("Test window", 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
	HWND hWnd;
#ifdef MINIWIN
	hWnd = reinterpret_cast<HWND>(window);
#else
	hWnd = (HWND) SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#endif
	if (m_device_enumerator->DoEnumerate(hWnd)) {
		return FALSE;
	}
	SDL_DestroyWindow(window);
	m_aspect_ratio = 0;
	m_exf_x_res = m_x_res = 640;
	m_exf_y_res = m_y_res = 480;
	m_exf_fps = 60.00f;
	m_frame_delta = 10.0f;
	m_driver = NULL;
	m_device = NULL;
	m_full_screen = TRUE;
	m_exclusive_full_screen = FALSE;
	m_transition_type = 3; // 3: Mosaic
	m_wide_view_angle = TRUE;
	m_use_joystick = TRUE;
	m_music = TRUE;
	m_flip_surfaces = FALSE;
	m_3d_video_ram = FALSE;
	m_joystick_index = -1;
	m_display_bit_depth = 16;
	m_msaa = 1;
	m_anisotropy = 1;
	m_lighting_model = 0;
	m_haptic = TRUE;
	m_wasd = FALSE;
	m_touch_scheme = 2;
	m_texture_load = TRUE;
	m_texture_path = "textures/";
	m_custom_assets_enabled = TRUE;
	m_custom_asset_path = "assets/widescreen.si";
	int totalRamMiB = SDL_GetSystemRAM();
	if (totalRamMiB < 12) {
		m_ram_quality_limit = 2;
		m_3d_sound = FALSE;
		m_model_quality = 0;
		m_texture_quality = 1;
		m_max_lod = 1.5f;
		m_max_actors = 5;
	}
	else if (totalRamMiB < 20) {
		m_ram_quality_limit = 1;
		m_3d_sound = FALSE;
		m_model_quality = 1;
		m_texture_quality = 1;
		m_max_lod = 2.5f;
		m_max_actors = 10;
	}
	else {
		m_ram_quality_limit = 0;
		m_model_quality = 2;
		m_3d_sound = TRUE;
		m_texture_quality = 1;
		m_max_lod = 3.5f;
		m_max_actors = 20;
	}
	return true;
}

// FUNCTION: CONFIG 0x004033d0
bool CConfigApp::IsDeviceInBasicRGBMode() const
{
	/*
	 * BUG: should be:
	 *  return !GetHardwareDeviceColorModel() && (m_device->m_HELDesc.dcmColorModel & D3DCOLOR_RGB);
	 */
	return GetHardwareDeviceColorModel() == D3DCOLOR_NONE && m_device->m_HELDesc.dcmColorModel == D3DCOLOR_RGB;
}

// FUNCTION: CONFIG 0x00403400
D3DCOLORMODEL CConfigApp::GetHardwareDeviceColorModel() const
{
	return m_device->m_HWDesc.dcmColorModel;
}

// FUNCTION: CONFIG 0x00403410
bool CConfigApp::IsPrimaryDriver() const
{
	return m_driver == &m_device_enumerator->m_ddInfo.front();
}

// FUNCTION: CONFIG 0x00403430
bool CConfigApp::ReadRegisterSettings()
{
	int tmp = -1;
#define NOT_FOUND (-2)

	dictionary* dict = iniparser_load(m_iniPath.c_str());
	if (!dict) {
		if (SDL_GetPathInfo(m_iniPath.c_str(), nullptr)) {
			// Showing defaults over a file that is still there would invite the user to configure
			// everything and only find out at Save, which refuses rather than replacing it.
			QMessageBox::warning(
				nullptr,
				"Could not read settings",
				QString::fromStdString(m_iniPath) +
					" could not be read, so these are defaults. Saving will not overwrite it; repair or "
					"remove the file first."
			);
		}
		dict = dictionary_new(0);
	}

	const char* device3D = iniparser_getstring(dict, "isle:3D Device ID", nullptr);
	if (device3D) {
		tmp = m_device_enumerator->ParseDeviceName(device3D);
		if (tmp >= 0) {
			tmp = m_device_enumerator->GetDevice(tmp, m_driver, m_device);
		}
	}
	if (tmp != 0) {
		m_device_enumerator->FUN_1009d210();
		tmp = m_device_enumerator->GetBestDevice();
		m_device_enumerator->GetDevice(tmp, m_driver, m_device);
	}
	m_base_path = iniparser_getstring(dict, "isle:diskpath", m_base_path.c_str());
	m_cd_path = iniparser_getstring(dict, "isle:cdpath", m_cd_path.c_str());
	m_save_path = iniparser_getstring(dict, "isle:savepath", m_save_path.c_str());
	m_display_bit_depth = iniparser_getint(dict, "isle:Display Bit Depth", -1);
	m_flip_surfaces = iniparser_getboolean(dict, "isle:Flip Surfaces", m_flip_surfaces);
	m_full_screen = iniparser_getboolean(dict, "isle:Full Screen", m_full_screen);
	m_exclusive_full_screen = iniparser_getboolean(dict, "isle:Exclusive Full Screen", m_exclusive_full_screen);
	m_transition_type = iniparser_getint(dict, "isle:Transition Type", m_transition_type);
	m_touch_scheme = iniparser_getint(dict, "isle:Touch Scheme", m_touch_scheme);
	m_3d_video_ram = iniparser_getboolean(dict, "isle:Back Buffers in Video RAM", m_3d_video_ram);
	m_wide_view_angle = iniparser_getboolean(dict, "isle:Wide View Angle", m_wide_view_angle);
	m_3d_sound = iniparser_getboolean(dict, "isle:3DSound", m_3d_sound);
	m_draw_cursor = iniparser_getboolean(dict, "isle:Draw Cursor", m_draw_cursor);
	m_model_quality = iniparser_getint(dict, "isle:Island Quality", m_model_quality);
	m_texture_quality = iniparser_getint(dict, "isle:Island Texture", m_texture_quality);
	m_use_joystick = iniparser_getboolean(dict, "isle:UseJoystick", m_use_joystick);
	m_haptic = iniparser_getboolean(dict, "isle:Haptic", m_haptic);
	m_wasd = iniparser_getboolean(dict, "isle:WASD", m_wasd);
	m_music = iniparser_getboolean(dict, "isle:Music", m_music);
	m_joystick_index = iniparser_getint(dict, "isle:JoystickIndex", m_joystick_index);
	m_max_lod = iniparser_getdouble(dict, "isle:Max LOD", m_max_lod);
	m_max_actors = iniparser_getint(dict, "isle:Max Allowed Extras", m_max_actors);
	m_msaa = iniparser_getint(dict, "isle:MSAA", m_msaa);
	m_lighting_model = iniparser_getint(dict, "isle:Lighting Model", m_lighting_model);
	m_anisotropy = iniparser_getint(dict, "isle:Anisotropic", m_anisotropy);
	m_texture_load = iniparser_getboolean(dict, "extensions:texture loader", m_texture_load);
	m_texture_path = iniparser_getstring(dict, "texture loader:texture path", m_texture_path.c_str());
	m_custom_assets_enabled = iniparser_getboolean(dict, "extensions:si loader", m_custom_assets_enabled);
	m_custom_asset_path = iniparser_getstring(dict, "si loader:files", m_custom_asset_path.c_str());
	m_aspect_ratio = iniparser_getint(dict, "isle:Aspect Ratio", m_aspect_ratio);
	m_x_res = iniparser_getint(dict, "isle:Horizontal Resolution", m_x_res);
	m_y_res = iniparser_getint(dict, "isle:Vertical Resolution", m_y_res);
	m_exf_x_res = iniparser_getint(dict, "isle:Exclusive X Resolution", m_exf_x_res);
	m_exf_y_res = iniparser_getint(dict, "isle:Exclusive Y Resolution", m_exf_y_res);
	m_exf_fps = iniparser_getdouble(dict, "isle:Exclusive Framerate", m_exf_fps);
	m_frame_delta = iniparser_getdouble(dict, "isle:Frame Delta", m_frame_delta);
	iniparser_freedict(dict);
	return true;
}

// FUNCTION: CONFIG 0x00403630
bool CConfigApp::ValidateSettings()
{
	BOOL is_modified = FALSE;

	if (IsDeviceInBasicRGBMode()) {
		if (m_3d_video_ram) {
			m_3d_video_ram = FALSE;
			is_modified = TRUE;
		}
		if (m_flip_surfaces) {
			m_flip_surfaces = FALSE;
			is_modified = TRUE;
		}
		if (m_display_bit_depth != 16) {
			m_display_bit_depth = 16;
			is_modified = TRUE;
		}
	}
	if (GetHardwareDeviceColorModel() == D3DCOLOR_NONE) {
		m_draw_cursor = FALSE;
		is_modified = TRUE;
	}
	else {
		if (!m_3d_video_ram) {
			m_3d_video_ram = TRUE;
			is_modified = TRUE;
		}
		if (m_full_screen && !m_flip_surfaces) {
			m_flip_surfaces = TRUE;
			is_modified = TRUE;
		}
	}
	if (m_flip_surfaces) {
		if (!m_3d_video_ram) {
			m_3d_video_ram = TRUE;
			is_modified = TRUE;
		}
	}
	if ((m_display_bit_depth != 8 && m_display_bit_depth != 16) && (m_display_bit_depth != 0 || m_full_screen)) {
		m_display_bit_depth = 16;
		is_modified = TRUE;
	}
	if (m_model_quality < 0 || m_model_quality > 2) {
		m_model_quality = 1;
		is_modified = TRUE;
	}
	if (m_texture_quality < 0 || m_texture_quality > 1) {
		m_texture_quality = 0;
		is_modified = TRUE;
	}

	if (m_max_lod < 0.0f || m_max_lod > 6.0f) {
		m_max_lod = 3.5f;
		is_modified = TRUE;
	}
	if (m_max_actors < 5 || m_max_actors > 40) {
		m_max_actors = 20;
		is_modified = TRUE;
	}
	if (!m_use_joystick) {
		m_use_joystick = true;
		is_modified = TRUE;
	}
	if (m_touch_scheme < 0 || m_touch_scheme > 2) {
		m_touch_scheme = 2;
		is_modified = TRUE;
	}
	if (m_exclusive_full_screen && !m_full_screen) {
		m_full_screen = TRUE;
		is_modified = TRUE;
	}
	if (m_lighting_model < 0 || m_lighting_model > 1) {
		m_lighting_model = 0;
		is_modified = TRUE;
	}
	if (!(m_msaa & (m_msaa - 1))) {         // Check if MSAA is power of 2 (1, 2, 4, 8, etc)
		m_msaa = exp2(round(log2(m_msaa))); // Closest power of 2
		is_modified = TRUE;
	}
	if (m_msaa > 16) {
		m_msaa = 16;
		is_modified = TRUE;
	}
	else if (m_msaa < 1) {
		m_msaa = 1;
		is_modified = TRUE;
	}
	if (!(m_anisotropy & (m_anisotropy - 1))) {         // Check if anisotropy is power of 2 (1, 2, 4, 8, etc)
		m_anisotropy = exp2(round(log2(m_anisotropy))); // Closest power of 2
		is_modified = TRUE;
	}
	if (m_anisotropy > 16) {
		m_anisotropy = 16;
		is_modified = TRUE;
	}
	else if (m_anisotropy < 1) {
		m_anisotropy = 1;
		is_modified = TRUE;
	}

	return is_modified;
}

// FUNCTION: CONFIG 0x004037a0
DWORD CConfigApp::GetConditionalDeviceRenderBitDepth() const
{
	if (IsDeviceInBasicRGBMode()) {
		return 0;
	}
	if (GetHardwareDeviceColorModel() != D3DCOLOR_NONE) {
		return 0;
	}
	return (m_device->m_HELDesc.dwDeviceRenderBitDepth & DDBD_8) == DDBD_8;
}

// FUNCTION: CONFIG 0x004037e0
DWORD CConfigApp::GetDeviceRenderBitStatus() const
{
	if (GetHardwareDeviceColorModel() != D3DCOLOR_NONE) {
		return (m_device->m_HWDesc.dwDeviceRenderBitDepth & DDBD_16) == DDBD_16;
	}
	else {
		return (m_device->m_HELDesc.dwDeviceRenderBitDepth & DDBD_16) == DDBD_16;
	}
}

// FUNCTION: CONFIG 0x00403810
bool CConfigApp::AdjustDisplayBitDepthBasedOnRenderStatus()
{
	if (m_display_bit_depth == 8) {
		if (GetConditionalDeviceRenderBitDepth()) {
			return FALSE;
		}
	}
	if (m_display_bit_depth == 16) {
		if (GetDeviceRenderBitStatus()) {
			return FALSE;
		}
	}
	if (GetConditionalDeviceRenderBitDepth()) {
		m_display_bit_depth = 8;
		return TRUE;
	}
	if (GetDeviceRenderBitStatus()) {
		m_display_bit_depth = 16;
		return TRUE;
	}
	m_display_bit_depth = 16;
	return TRUE;
}

// FUNCTION: CONFIG 00403890
bool CConfigApp::WriteRegisterSettings() const

{
	char buffer[128];

	// iniparser reads a line back through a 1024 byte buffer and fails the whole load when one is
	// longer, so a path that will not fit costs every other setting in the file. Refuse it here,
	// while the file on disk is still the good one.
	const struct {
		const char* m_key;
		const std::string& m_value;
	} texts[] = {
		{"isle:diskpath", m_base_path},
		{"isle:cdpath", m_cd_path},
		{"isle:savepath", m_save_path},
		{"texture loader:texture path", m_texture_path},
		{"si loader:files", m_custom_asset_path},
	};
	for (const auto& text : texts) {
		if (!IniFile::FitsOnOneLine(text.m_key, text.m_value.c_str())) {
			QMessageBox::warning(
				nullptr,
				"Could not save settings",
				QString("The value for \"%1\" is too long for the configuration file.").arg(text.m_key)
			);
			return false;
		}
	}

	// Keep the keys this tool does not own. The same isle.ini holds the gamepad bindings, the
	// touch layout and the extension settings, and rebuilding the file from the keys below would
	// delete all of them; only the keys this dialog edits are set. Keys are all that survives:
	// iniparser's loader drops comments, blank lines and the original key order and case, and
	// anything written above the first [section] with it.
	dictionary* dict = iniparser_load(m_iniPath.c_str());
	if (!dict) {
		if (SDL_GetPathInfo(m_iniPath.c_str(), nullptr)) {
			// It exists and will not parse. Writing now would replace settings that are still
			// there to be repaired, so say so and leave the file alone.
			QMessageBox::warning(
				nullptr,
				"Could not save settings",
				QString::fromStdString(m_iniPath) + " could not be read, so it has not been changed."
			);
			return false;
		}
		// Nothing there yet: the first run writes a fresh configuration.
		dict = dictionary_new(0);
	}

#define SetIniBool(DICT, NAME, VALUE) iniparser_set(DICT, NAME, VALUE ? "true" : "false")
#define SetIniInt(DICT, NAME, VALUE)                                                                                   \
	do {                                                                                                               \
		sprintf(buffer, "%d", VALUE);                                                                                  \
		iniparser_set(DICT, NAME, buffer);                                                                             \
	} while (0)

	m_device_enumerator->FormatDeviceName(buffer, m_driver, m_device);

	iniparser_set(dict, "isle", NULL);
	iniparser_set(dict, "extensions", NULL);
	iniparser_set(dict, "texture loader", NULL);
	iniparser_set(dict, "si loader", NULL);

	if (m_device_enumerator->FormatDeviceName(buffer, m_driver, m_device) >= 0) {
		iniparser_set(dict, "isle:3D Device ID", buffer);
	}
	else {
		// The only key written conditionally, so the only one the merge would otherwise make
		// permanent. Leaving a device id that no longer names anything here would keep the game
		// from falling back to the best device it can find.
		iniparser_unset(dict, "isle:3D Device ID");
	}
	iniparser_set(dict, "isle:diskpath", m_base_path.c_str());
	iniparser_set(dict, "isle:cdpath", m_cd_path.c_str());
	iniparser_set(dict, "isle:savepath", m_save_path.c_str());

	SetIniInt(dict, "isle:Display Bit Depth", m_display_bit_depth);
	SetIniInt(dict, "isle:MSAA", m_msaa);
	SetIniInt(dict, "isle:Lighting Model", m_lighting_model);
	SetIniInt(dict, "isle:Anisotropic", m_anisotropy);
	SetIniBool(dict, "isle:Flip Surfaces", m_flip_surfaces);
	SetIniBool(dict, "isle:Full Screen", m_full_screen);
	SetIniBool(dict, "isle:Exclusive Full Screen", m_exclusive_full_screen);
	SetIniBool(dict, "isle:Wide View Angle", m_wide_view_angle);

	SetIniInt(dict, "isle:Transition Type", m_transition_type);
	SetIniInt(dict, "isle:Touch Scheme", m_touch_scheme);

	SetIniBool(dict, "isle:3DSound", m_3d_sound);
	SetIniBool(dict, "isle:Music", m_music);
	SetIniBool(dict, "isle:Haptic", m_haptic);
	SetIniBool(dict, "isle:WASD", m_wasd);

	SetIniBool(dict, "isle:UseJoystick", m_use_joystick);
	SetIniInt(dict, "isle:JoystickIndex", m_joystick_index);
	SetIniBool(dict, "isle:Draw Cursor", m_draw_cursor);

	SetIniBool(dict, "extensions:texture loader", m_texture_load);
	iniparser_set(dict, "texture loader:texture path", m_texture_path.c_str());

	SetIniBool(dict, "extensions:si loader", m_custom_assets_enabled);
	iniparser_set(dict, "si loader:files", m_custom_asset_path.c_str());

	SetIniBool(dict, "isle:Back Buffers in Video RAM", m_3d_video_ram);

	SetIniInt(dict, "isle:Island Quality", m_model_quality);
	SetIniInt(dict, "isle:Island Texture", m_texture_quality);

	iniparser_set(dict, "isle:Max LOD", std::to_string(m_max_lod).c_str());
	SetIniInt(dict, "isle:Max Allowed Extras", m_max_actors);

	SetIniInt(dict, "isle:Aspect Ratio", m_aspect_ratio);
	SetIniInt(dict, "isle:Horizontal Resolution", m_x_res);
	SetIniInt(dict, "isle:Vertical Resolution", m_y_res);
	SetIniInt(dict, "isle:Exclusive X Resolution", m_exf_x_res);
	SetIniInt(dict, "isle:Exclusive Y Resolution", m_exf_y_res);
	iniparser_set(dict, "isle:Exclusive Framerate", std::to_string(m_exf_fps).c_str());
	iniparser_set(dict, "isle:Frame Delta", std::to_string(m_frame_delta).c_str());

#undef SetIniBool
#undef SetIniInt

	// The merge brought in values this dialog never wrote. One of them can be longer than a line
	// once the dumper pads its name and doubles its backslashes, and writing it would leave a
	// file nothing can read - the opposite of what keeping those keys was for.
	const std::string overlong = IniFile::FindOverlongEntry(dict);
	if (!overlong.empty()) {
		iniparser_freedict(dict);
		QMessageBox::warning(
			nullptr,
			"Could not save settings",
			QString("The existing value for \"%1\" is too long for the configuration file, so it has not "
					"been changed. Shorten it and try again.")
				.arg(QString::fromStdString(overlong))
		);
		return false;
	}

	const std::string error = IniFile::Save(m_iniPath, dict);
	iniparser_freedict(dict);
	if (!error.empty()) {
		QMessageBox::warning(nullptr, "Could not save settings", QString::fromStdString(error));
		return false;
	}
	qInfo() << "New config written at" << QString::fromStdString(m_iniPath);
	return true;
}

// FUNCTION: CONFIG 0x00403a90
int CConfigApp::ExitInstance()
{
	if (m_device_enumerator) {
		delete m_device_enumerator;
		m_device_enumerator = NULL;
	}
	SDL_Quit();
	return 0;
}

void CConfigApp::SetIniPath(const std::string& p_path)
{
	m_iniPath = p_path;
}
const std::string& CConfigApp::GetIniPath() const
{
	return m_iniPath;
}

// GLOBAL: CONFIG 0x00408e50
CConfigApp g_theApp;

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QGuiApplication::setDesktopFileName("org.legoisland.Isle");
	QCoreApplication::setApplicationName("Isle-Config");
	QCoreApplication::setApplicationVersion("2.0");

	QCommandLineParser parser;
	parser.setApplicationDescription("Configure LEGO Island");
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption iniOption(
		QStringList() << "ini",
		QCoreApplication::translate("config", "Set INI path."),
		QCoreApplication::translate("config", "path")
	);
	parser.addOption(iniOption);
	parser.process(app);

	if (parser.isSet(iniOption)) {
		g_theApp.SetIniPath(parser.value(iniOption).toStdString());
	}
	qInfo() << "INI path =" << QString::fromStdString(g_theApp.GetIniPath());

	int result = 1;
	if (g_theApp.InitInstance()) {
		CMainDialog main_dialog;
		main_dialog.show();
		result = app.exec();
	}
	g_theApp.ExitInstance();
	return result;
}
