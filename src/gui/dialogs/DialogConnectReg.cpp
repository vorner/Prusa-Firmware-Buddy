#include "DialogConnectReg.hpp"
#include "../png_resources.hpp"
#include "../ScreenHandler.hpp"

namespace {

const PhaseResponses dlg_responses = { Response::Continue, Response::_none, Response::_none, Response::_none };

}

bool DialogConnectRegister::DialogShown = false;

DialogConnectRegister::DialogConnectRegister()
    : AddSuperWindow<IDialog>(WizardDefaults::RectSelftestFrame)
    , header(this, _(headerLabel))
    , icon_phone(this, Positioner::phoneIconRect(), &png::hand_qr_59x72)
    , qr(this, Positioner::qrcodeRect(), "Test test")
    , text(this, Positioner::textRect(), is_multiline::yes)
    , button(this, WizardDefaults::RectRadioButton(0), dlg_responses, &ph_txt_continue) {
    DialogShown = true;
    text.SetText(_("Lorem ipsum"));
}

DialogConnectRegister::~DialogConnectRegister() {
    DialogShown = false;
    // TODO: Leave connect registration
}

void DialogConnectRegister::Show() {
    DialogConnectRegister dialog;
    dialog.MakeBlocking();
}

void DialogConnectRegister::windowEvent(EventLock, window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::CHILD_CLICK:
    case GUI_event_t::CLICK:
        // We have a single button, so this simplification should work fine.
        Screens::Access()->Close();
        break;
    default:
        SuperWindowEvent(sender, event, param);
        break;
    }
}

constexpr Rect16 DialogConnectRegister::Positioner::qrcodeRect() {
    if (GuiDefaults::ScreenWidth > 240) {
        return Rect16 {
            GuiDefaults::ScreenWidth - WizardDefaults::MarginRight - qrcodeWidth,
            WizardDefaults::row_0,
            qrcodeWidth,
            qrcodeHeight
        };
    } else {
        return Rect16 { 160 - qrcodeWidth / 2, 200 - qrcodeHeight / 2, qrcodeWidth, qrcodeHeight };
    }
}

/** @returns Rect16 position and size of the phone icon widget */
constexpr Rect16 DialogConnectRegister::Positioner::phoneIconRect() {
    if (GuiDefaults::ScreenWidth > 240) {
        return Rect16 {
            qrcodeRect().Left() - phoneWidth,
            (qrcodeRect().Top() + qrcodeRect().Bottom()) / 2 - phoneHeight / 2,
            phoneWidth,
            phoneHeight
        };
    } else {
        return Rect16 { 20, 165, phoneWidth, phoneHeight };
    }
}

/** @returns Rect16 position and size of the text widget */
constexpr Rect16 DialogConnectRegister::Positioner::textRect() {
    if (GuiDefaults::ScreenWidth > 240) {
        return Rect16 { WizardDefaults::col_0, WizardDefaults::row_1, textWidth, textHeight };
    } else {
        return Rect16 { WizardDefaults::col_0, WizardDefaults::row_0, textWidth, textHeight };
    }
}
