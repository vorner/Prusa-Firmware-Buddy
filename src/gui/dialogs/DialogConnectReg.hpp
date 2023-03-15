#pragma once

#include <gui.hpp>
#include <window_header.hpp>
#include <window_qr.hpp>
#include "radio_button.hpp"

#include <connect/status.hpp>

class DialogConnectRegister : public AddSuperWindow<IDialog> {
private:
    static bool DialogShown;

    // TODO: Doesn't fit
    constexpr static const char *const headerLabel = N_("PRUSA CONNECT");

    // TODO: Stolen from selftest_frame_esp_qr.hpp ‒ unify to a common place.
    /** @brief Calculates the position of individual elements of the frame
     *
     * Resulting layout depends on GuiDefaults(ScreenWidth & ScreenHeight)
     * and WizardDefaults. Can be different on differently sized screens.
     *
     * Layout should be compliant with BFW-2561, but not pixel-perfect.
     *
     * Beware of the cyclic dependencies!
     */
    class Positioner {
        static constexpr size_t qrcodeWidth { 140 };
        static constexpr size_t qrcodeHeight { 140 };
        static constexpr size_t phoneWidth { 64 };
        static constexpr size_t phoneHeight { 82 };
        static constexpr size_t textWidth { WizardDefaults::X_space };
        static constexpr size_t textHeight { WizardDefaults::txt_h * 4 };

    public:
        /** @returns Rect16 position and size of QR Code widget */
        static constexpr Rect16 qrcodeRect();

        /** @returns Rect16 position and size of the phone icon widget */
        static constexpr Rect16 phoneIconRect();

        /** @returns Rect16 position and size of the text widget */
        static constexpr Rect16 textRect();
    };

    connect_client::OnlineStatus last_seen_status = connect_client::OnlineStatus::Unknown;

    window_header_t header;
    window_icon_t icon_phone;
    window_qr_t qr;
    window_text_t text;
    RadioButton button;

    DialogConnectRegister();

protected:
    virtual void windowEvent(EventLock /*has private ctor*/, window_t *sender, GUI_event_t event, void *param) override;

public:
    static void Show();
    ~DialogConnectRegister();
};
