#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Choice.H>
#include <string>
#include <cstdio>
#include <cstring>

Fl_Text_Buffer *buffer = NULL;
Fl_Text_Editor *editor = NULL;
Fl_Window *ai_window = NULL;
Fl_Input *prompt_input = NULL;
Fl_Multiline_Output *ai_output = NULL;

// Clean ANSI escape codes from text
std::string clean_ansi_codes(const std::string &text)
{
    std::string result;
    bool in_escape = false;

    for (size_t i = 0; i < text.length(); i++)
    {
        if (text[i] == '\x1B' || text[i] == '\x9B')
        {
            in_escape = true;
            continue;
        }

        if (in_escape)
        {
            if ((text[i] >= 'A' && text[i] <= 'Z') ||
                (text[i] >= 'a' && text[i] <= 'z'))
            {
                in_escape = false;
            }
            continue;
        }

        // Skip other control characters
        if (text[i] < 32 && text[i] != '\n' && text[i] != '\t')
        {
            continue;
        }

        result += text[i];
    }

    return result;
}

// Call Ollama with clean output
std::string call_ollama(const std::string &prompt)
{
    const char *response_file = "ollama_response.txt";
    const char *prompt_file = "ollama_prompt.txt";

    // Save prompt
    FILE *pf = fopen(prompt_file, "w");
    if (!pf)
        return "Error: Cannot create prompt file";
    fprintf(pf, "%s", prompt.c_str());
    fclose(pf);

    // Call Ollama - redirect stderr to hide progress
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "ollama run phi3:mini < %s > %s 2>nul",
             prompt_file, response_file);

    int result = system(cmd);

    if (result != 0)
    {
        return "Error: Ollama failed.\nMake sure it's installed: https://ollama.com/download";
    }

    // Read response
    FILE *rf = fopen(response_file, "r");
    if (!rf)
        return "Error: Cannot read response";

    char raw_response[50000];
    size_t len = fread(raw_response, 1, sizeof(raw_response) - 1, rf);
    raw_response[len] = '\0';
    fclose(rf);

    // Clean up files
    remove(prompt_file);
    remove(response_file);

    // Clean ANSI codes
    std::string cleaned = clean_ansi_codes(std::string(raw_response));

    // Trim whitespace
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");

    if (start != std::string::npos && end != std::string::npos)
    {
        return cleaned.substr(start, end - start + 1);
    }

    return cleaned;
}

void insert_ai_text(const std::string &text)
{
    int pos = editor->insert_position();
    buffer->insert(pos, ("\n" + text + "\n").c_str());
    editor->insert_position(pos + text.length() + 2);
    editor->show_insert_position();
}

void send_prompt_cb(Fl_Widget *, void *)
{
    const char *prompt = prompt_input->value();

    if (!prompt || strlen(prompt) == 0)
    {
        fl_alert("Please enter a prompt!");
        return;
    }

    ai_output->value("🤖 Generating... (may take 15-30 seconds)");
    ai_output->redraw();
    Fl::check();
    Fl::flush();

    // Call AI
    std::string response = call_ollama(prompt);

    // Display clean response
    ai_output->value(response.c_str());

    // Ask to insert
    int choice = fl_choice("Insert into editor?", "Cancel", "Insert", NULL);
    if (choice == 1)
    {
        insert_ai_text(response);
    }

    prompt_input->value("");
}

void show_ai_assistant(Fl_Widget *, void *)
{
    if (!ai_window)
    {
        ai_window = new Fl_Window(700, 600, "🤖 AI Assistant (100% Free)");

        Fl_Box *title = new Fl_Box(10, 10, 680, 35);
        title->label("✨ Free Local AI - Unlimited Usage - Works Offline");
        title->labelfont(FL_HELVETICA_BOLD);
        title->labelsize(14);
        title->color(fl_rgb_color(40, 200, 120));
        title->box(FL_FLAT_BOX);
        title->labelcolor(FL_WHITE);

        Fl_Box *info = new Fl_Box(10, 50, 680, 60);
        info->label("Try asking:\n"
                    "• Write a motivational quote\n"
                    "• Create a Python function to reverse a string\n"
                    "• Explain machine learning in simple terms");
        info->labelsize(11);
        info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        info->box(FL_BORDER_BOX);

        prompt_input = new Fl_Input(10, 130, 580, 35, "Your prompt:");
        prompt_input->align(FL_ALIGN_TOP_LEFT);
        prompt_input->textsize(12);

        Fl_Button *send_btn = new Fl_Button(600, 130, 90, 35, "Generate");
        send_btn->color(fl_rgb_color(0, 150, 255));
        send_btn->labelcolor(FL_WHITE);
        send_btn->labelfont(FL_HELVETICA_BOLD);
        send_btn->callback(send_prompt_cb);

        ai_output = new Fl_Multiline_Output(10, 185, 680, 385, "AI Response:");
        ai_output->align(FL_ALIGN_TOP_LEFT);
        ai_output->textsize(12);
        ai_output->textfont(FL_COURIER);

        ai_window->end();
    }
    ai_window->show();
}

void ai_quote_cb(Fl_Widget *, void *)
{
    fl_message("Generating quote... (15-30 seconds)");
    std::string quote = call_ollama("Generate a short inspiring quote about programming and technology. Keep it under 50 words.");
    insert_ai_text(quote);
    fl_message("Quote inserted!");
}

void ai_blog_cb(Fl_Widget *, void *)
{
    const char *topic = fl_input("Blog topic:", "Future of AI");
    if (topic)
    {
        fl_message("Writing blog... (30-60 seconds)\nPlease wait...");
        std::string prompt = std::string("Write a 200-word blog post about: ") + topic;
        std::string blog = call_ollama(prompt);
        insert_ai_text(blog);
        fl_message("Blog inserted!");
    }
}

void ai_code_cb(Fl_Widget *, void *)
{
    const char *desc = fl_input("What function to create?", "reverse a string");
    if (desc)
    {
        fl_message("Generating code... (20-40 seconds)");
        std::string prompt = std::string("Write a Python function to ") + desc +
                             ". Include comments and a usage example.";
        std::string code = call_ollama(prompt);
        insert_ai_text(code);
        fl_message("Code inserted!");
    }
}

void ai_explain_cb(Fl_Widget *, void *)
{
    if (!buffer->selected())
    {
        fl_alert("Please select some text first!");
        return;
    }

    char *selected = buffer->selection_text();
    std::string prompt = std::string("Explain this briefly:\n\n") + selected;
    free(selected);

    fl_message("Analyzing... (15-30 seconds)");
    std::string explanation = call_ollama(prompt);

    fl_message_title("AI Explanation");
    fl_message("%s", explanation.c_str());
}

void quit_cb(Fl_Widget *, void *) { exit(0); }

int main()
{
    Fl::scheme("gtk+");

    Fl_Window *win = new Fl_Window(1000, 700, "🤖 Free AI Editor");

    Fl_Menu_Bar *menu = new Fl_Menu_Bar(0, 0, 1000, 25);
    menu->add("File/Quit", FL_COMMAND + 'q', quit_cb);
    menu->add("AI/🤖 AI Assistant", FL_COMMAND + 'a', show_ai_assistant);
    menu->add("AI/Quick: Quote", FL_COMMAND + '1', ai_quote_cb);
    menu->add("AI/Quick: Blog", FL_COMMAND + '2', ai_blog_cb);
    menu->add("AI/Quick: Code", FL_COMMAND + '3', ai_code_cb);
    menu->add("AI/Explain Selection", FL_COMMAND + 'e', ai_explain_cb);

    buffer = new Fl_Text_Buffer();
    editor = new Fl_Text_Editor(0, 25, 1000, 645);
    editor->buffer(buffer);
    editor->textfont(FL_COURIER);
    editor->textsize(13);
    editor->linenumber_width(40);

    buffer->text(
        "# 🤖 Free AI Editor - Clean Output Version\n\n"
        "AI Features (100% Free & Offline):\n"
        "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"
        "• Ctrl+A → AI Assistant\n"
        "• Ctrl+1 → Generate quote\n"
        "• Ctrl+2 → Write blog post\n"
        "• Ctrl+3 → Generate code\n"
        "• Ctrl+E → Explain selected text\n\n"
        "Now with CLEAN output (no weird characters!).\n"
        "Start using AI!\n");

    Fl_Box *status = new Fl_Box(0, 670, 1000, 30);
    status->box(FL_FLAT_BOX);
    status->color(fl_rgb_color(40, 200, 120));
    status->labelcolor(FL_WHITE);
    status->labelfont(FL_HELVETICA_BOLD);
    status->label("  🟢 Free AI Ready | Press Ctrl+A | Clean Output Version");
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    win->resizable(editor);
    win->end();
    win->show();

    return Fl::run();
}
