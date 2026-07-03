#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <infrared.h>
#include <infrared/infrared_signal.h>

#define TAG "VortexRemote"

// Structura aplicatiei
struct VortexRemoteApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    int selected_button;

    VortexRemoteApp() : gui(nullptr), view_port(nullptr), event_queue(nullptr), selected_button(0) {}
};

// Numele butoanelor din layout-ul telecomenzii
const char* button_labels[] = {
    "POWER", "SRC", "MUTE",
    "VOL+",  "CH+", "MENU",
    "VOL-",  "CH-", "EXIT",
    " UP ",  " OK ", "LEFT",
    "DOWN",  "RIGHT", "BACK"
};

// Codurile HEX pentru protocolul RCA (Address: 0F 00 00 00)
const uint32_t rca_commands[] = {
    0x54000000, 0xC5000000, 0xFC000000, // Power, Source, Mute
    0xF4000000, 0xB4000000, 0x10000000, // Vol+, Ch+, Menu
    0x74000000, 0x34000000, 0x60000000, // Vol-, Ch-, Exit
    0x9A000000, 0x2F000000, 0x6A000000, // Up, Ok, Left
    0x1A000000, 0xEA000000, 0xE4000000  // Down, Right, Return
};

// ---------------------------------------------------------------------
// Interfata grafica - stil inspirat din NetflixTvRemote (header plin,
// butoane cu colturi rotunjite, highlight curat pe selectie)
// ---------------------------------------------------------------------
static void render_callback(Canvas* canvas, void* ctx) {
    auto* app = static_cast<VortexRemoteApp*>(ctx);
    canvas_clear(canvas);

    // --- Header (bara plina, gen banner, ca la modelul Netflix) ---
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "VORTEX TV REMOTE");
    canvas_set_color(canvas, ColorBlack);

    // --- Grila de butoane (5 randuri x 3 coloane), colturi rotunjite ---
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 15; i++) {
        int row = i / 3;
        int col = i % 3;
        int x = 4 + col * 41;
        int y = 16 + row * 10;
        int w = 38;
        int h = 9;

        bool selected = (app->selected_button == i);

        if(selected) {
            // Buton selectat: fundal plin, colturi rotunjite, text alb
            canvas_draw_rbox(canvas, x, y, w, h, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            // Buton normal: contur rotunjit
            canvas_draw_rframe(canvas, x, y, w, h, 2);
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, button_labels[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

// Functie hardware reala pentru trimiterea semnalului IR (API real Flipper: InfraredSignal)
static void send_ir_signal(uint32_t command) {
    InfraredMessage message = {
        .protocol = infrared_get_protocol_by_name("RCA"),
        .address = 0x0F,
        .command = command,
        .repeat = false
    };

    InfraredSignal* signal = infrared_signal_alloc();
    infrared_signal_set_message(signal, &message);

    if(infrared_signal_is_valid(signal)) {
        infrared_signal_transmit(signal);
    } else {
        FURI_LOG_E(TAG, "Semnal IR invalid pentru comanda 0x%08lX", command);
    }

    infrared_signal_free(signal);
}

// Callback pentru input (butoane fizice)
static void input_callback(InputEvent* input_event, void* ctx) {
    auto* event_queue = static_cast<FuriMessageQueue*>(ctx);
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Entry point-ul aplicatiei
extern "C" int32_t vortex_remote_app(void* p) {
    UNUSED(p);
    auto* app = new VortexRemoteApp();

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    app->gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running && furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
            switch(event.key) {
            case InputKeyBack:
                // Pastrat identic cu functionalitatea originala: Back = iesire din aplicatie
                running = false;
                break;
            case InputKeyUp:
                if(app->selected_button >= 3) app->selected_button -= 3;
                break;
            case InputKeyDown:
                if(app->selected_button <= 11) app->selected_button += 3;
                break;
            case InputKeyLeft:
                if(app->selected_button % 3 > 0) app->selected_button--;
                break;
            case InputKeyRight:
                if(app->selected_button % 3 < 2) app->selected_button++;
                break;
            case InputKeyOk:
                send_ir_signal(rca_commands[app->selected_button]);
                break;
            default:
                break;
            }
            view_port_update(app->view_port);
        }
    }

    // Cleanup complet
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    delete app;

    return 0;
}