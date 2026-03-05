#include <utils/InputUtils.h>

/*
// Note: missing from WUT
constexpr uint32_t my_KPAD_BUTTON_REPEAT = 0x80000000;
constexpr uint32_t my_VPAD_BUTTON_REPEAT = 0x80000000;

// Note: missing form WUT
extern "C"
void KPADSetBtnRepeat(KPADChan channel,
                      float delay,
                      float period);
extern "C"
void KPADSetCrossStickEmulationParamsL(KPADChan channel,
                                       float rotationDegree,
                                       float range,
                                       float period);

*/
void Input::initialize() {
    return;
    /*
    KPADInit();

    WPADEnableURCC(1);

    KPADSetBtnRepeat(WPAD_CHAN_0, 0.5f, 0.1f);
    VPADSetBtnRepeat(VPAD_CHAN_0, 0.5f, 0.1f);

    KPADSetCrossStickEmulationParamsL(WPAD_CHAN_0, 0.0f, 45.0f, 0.75f);
    VPADSetCrossStickEmulationParamsL(VPAD_CHAN_0, 0.0f, 45.0f, 0.75f);
*/
}

void Input::finalize() {
    return;
    //KPADShutdown();
}

void Input::read() {
    return;
    //VPADRead(VPAD_CHAN_0, &vpad_status, 1, nullptr);
    //KPADRead(WPAD_CHAN_0, &kpad_status, 1);
}

bool Input::get([[maybe_unused]] ButtonState state, Button button) const {


    /*
    uint32_t vpadState        = 0;
    uint32_t kpadCoreState    = 0;
    uint32_t kpadClassicState = 0;
    uint32_t kpadNunchukState = 0;
    uint32_t kpadProState     = 0;

    auto examine = [state](const auto& status, bool is_vpad = false) -> uint32_t
    {
        switch (state) {
            case ButtonState::TRIGGER:
                return status.trigger;
            case ButtonState::HOLD:
                if (is_vpad)
                    return status.hold & ~my_VPAD_BUTTON_REPEAT;
                else
                    return status.hold & ~my_KPAD_BUTTON_REPEAT;
            case ButtonState::REPEAT:
                if (is_vpad) {
                    if (status.hold & my_VPAD_BUTTON_REPEAT)
                        return status.hold & ~my_VPAD_BUTTON_REPEAT;
                    else
                        return 0;
                } else {
                    if (status.hold & my_KPAD_BUTTON_REPEAT)
                        return status.hold & ~my_KPAD_BUTTON_REPEAT;
                    else
                        return 0;
                }
            case ButtonState::RELEASE:
                return status.release;
            default:
                return 0;
        }
    };

    if (!vpad_status.error)
        vpadState = examine(vpad_status, true);

    if (!kpad_status.error) {
        switch (kpad_status.extensionType) {
            case WPAD_EXT_CORE:
                kpadCoreState = examine(kpad_status);
                break;
            case WPAD_EXT_NUNCHUK:
                kpadCoreState    = examine(kpad_status);
                kpadNunchukState = examine(kpad_status.nunchuk);
                break;
            case WPAD_EXT_CLASSIC:
                kpadCoreState    = examine(kpad_status);
                kpadClassicState = examine(kpad_status.classic);
                break;
            case WPAD_EXT_PRO_CONTROLLER:
                kpadProState = examine(kpad_status.pro);
                break;
        }
    }

    if (!vpadState &&
        !kpadCoreState &&
        !kpadClassicState &&
        !kpadNunchukState &&
        !kpadProState)
        return false;
*/
    char pressed_key;

    if (buffer.size() > 0) {
        pressed_key = buffer.at(0);
    } else
        return false;

    switch (button) {
        case Button::A:
            if (pressed_key == 'a' || pressed_key == 'A')
                goto pressed;
            break;
        case Button::B:
            if (pressed_key == 'b' || pressed_key == 'B')
                goto pressed;
            break;
        case Button::X:
            if (pressed_key == 'x' || pressed_key == 'X')
                goto pressed;
            break;
        case Button::Y:
            if (pressed_key == 'y' || pressed_key == 'Y')
                goto pressed;
            break;
        case Button::UP:
            if (pressed_key == 'u' || pressed_key == 'U')
                goto pressed;
            break;
        case Button::DOWN:
            if (pressed_key == 'd' || pressed_key == 'D')
                goto pressed;
            break;
        case Button::LEFT:
            if (pressed_key == 'l')
                goto pressed;
            break;
        case Button::RIGHT:
            if (pressed_key == 'r')
                goto pressed;
            break;
        case Button::L:
            if (pressed_key == 'L')
                goto pressed;
            break;
        case Button::R:
            if (pressed_key == 'R')
                goto pressed;
            break;
        case Button::ZL:
            if (pressed_key == '<')
                goto pressed;
            break;
        case Button::ZR:
            if (pressed_key == '>')
                goto pressed;
            break;
        case Button::PLUS:
            if (pressed_key == '+')
                goto pressed;
            break;
        case Button::MINUS:
            if (pressed_key == '-')
                goto pressed;
            break;
        default:;
    }

    return false;

pressed:
    //std::vector<char>::const_iterator iter = buffer.begin();
    buffer.erase(buffer.begin());
    return true;
}


void Input::press(char key) {

    Input::buffer.push_back(key);
}