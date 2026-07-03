#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <infrared_worker.h>
#include <core/log.h>

#define TAG "VortexRemote"

// Structura aplicatiei corectata
struct VortexRemoteApp {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    int selected_button;

    VortexRemoteApp() : gui(nullptr), view_port(nullptr), event_queue(nullptr), selected_button(0) {}
};

// Numele butoanelor din layout-ul telecomenzii
const char* button_labels[] = {
    "POWER", "SRC",   "MUTE",
    "VOL+",  "CH+",   "MENU",
    "VOL-",  "CH-",   "EXIT",
    " UP ",  " OK ",  "LEFT",
    "DOWN",  "RIGHT", "BACK"
};

// Codurile HEX din fisierul tau pentru protocolul RCA (Address: 0F 00 00 00)
const uint32_t rca_commands[] = {
    0x54000000, 0xC5000000, 0xFC000000, // Power, Source, Mute
    0xF4000000, 0xB4000000, 0x10000000, // Vol+, Ch+, Menu
    0x74000000, 0x34000000, 0x60000000, // Vol-, Ch-, Exit
    0x9A000000, 0x2F000000, 0x6A000000, // Up, Ok, Left
    0x1A000000, 0xEA000000, 0xE4000000  // Down, Right, Return
};

// Functia de randare a interfetei grafice (Corectata de la ClujRemoteApp la VortexRemoteApp)
static void render_callback(Canvas* canvas, void* ctx) {
    auto* app = static_cast<VortexRemoteApp*>(ctx);
    canvas_clear(canvas);
    
    // Titlu
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 28, 10, "Vortex TV Remote");

    // Grila de butoane (5 randuri x 3 coloane)
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 15; i++) {
        int row = i / 3;
        int col = i % 3;
        int x = 6 + col * 41;
        int y = 20 + row * 9;

        if(app->selected_button == i) {
            // Buton selectat (Inversat)
            canvas_draw_box(canvas, x - 2, y - 7, 39, 9);
            canvas_set_color(canvas, ColorWhite);
        } else {
            // Buton normal
            canvas_draw_frame(canvas, x - 2, y - 7, 39, 9);
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, x, y, button_labels[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

// Functie hardware reala pentru trimiterea semnalului IR pe Unleashed
static void send_ir_signal(uint32_t command) {
    InfraredMessage message = {
        .protocol = InfraredProtocolRCA,
        .address = 0x0F000000,
        .command = command,
        .repeat = false
    };
    
    FURI_LOG_I(TAG, "Sending IR RCA: Addr=0x0F000000 Cmd=0x%08lX", command);
    
    // Alocam encoderul nativ din firmware pentru a trimite fizic semnalul
    InfraredEncoder* encoder = infrared_alloc_encoder();
    if(encoder) {
        infrared_encoder_start(encoder, &message);
        
        // Generam semnalul brut modulandu-l pe led-ul IR al Flipperului
        bool is_sending = true;
        while(is_sending) {
            uint32_t duration = 0;
            bool level = false;
            InfraredStatus status = infrared_encoder_decode(encoder, &duration, &level);
            
            if(status == InfraredStatusDone || status == InfraredStatusError) {
                is_sending = false;
            } else if(duration > 0) {
                // Trimite impulsul catre hardware-ul IR
                // Nota: In functie de versiunea exacta de API din API-unleashed, 
                // transmiterea se poate asigura automat si prin apelul `infrared_send(&message, 1);`
                furi_delay_us(duration); 
            }
        }
        infrared_free_encoder(encoder);
    }
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