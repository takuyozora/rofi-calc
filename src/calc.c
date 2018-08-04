/**
 * rofi-calc
 *
 * MIT/X11 License
 * Copyright (c) 2018 Sven-Hendrik Haase <svenstaro@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <gmodule.h>
#include <gio/gio.h>

#include <rofi/mode.h>
#include <rofi/helper.h>
#include <rofi/mode-private.h>

#include <stdint.h>

#define ANS "ans"

G_MODULE_EXPORT Mode mode;

/**
 * The internal data structure holding the private data of the TEST Mode.
 */
typedef struct
{
    char* last_result;
    GPtrArray* history;
} CALCModePrivateData;

static void get_calc(Mode* sw)
{
    /**
     * Get the entries to display.
     * this gets called on plugin initialization.
     */
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);
    pd->last_result = g_strdup("");
    pd->history = g_ptr_array_new();
}


static int calc_mode_init(Mode* sw)
{
    /**
     * Called on startup when enabled (in modi list)
     */
    if (mode_get_private_data(sw) == NULL) {
        CALCModePrivateData* pd = g_malloc0(sizeof(*pd));
        mode_set_private_data(sw, (void*)pd);
        // Load content.
        get_calc(sw);
    }
    return TRUE;
}


static unsigned int calc_mode_get_num_entries(const Mode* sw)
{
    const CALCModePrivateData* pd = (const CALCModePrivateData*)mode_get_private_data(sw);

    // Add +1 because we put a static message into the history array as well.
    return pd->history->len + 2;
}


static gboolean is_error_string(char* str)
{

    if (g_strrstr(str, "warning:") != NULL || g_strrstr(str, "error:") != NULL) {
        return TRUE;
    }
    return FALSE;
}

static int get_real_history_index(GPtrArray* history, unsigned int selected_line)
{
    // +1 because of the two command line displayed (copy to clipboard and add to history)
    return history->len - selected_line + 1;
}


static const char* get_only_result_part(const char** string){
    if (*string == NULL){
        return (const char *)1;
    }
    const char* r = strchr(*string, '=');
    if (r == NULL){
        return (const char *)2;
    }
    return &r[2]    ;
}


static void copy_only_result_to_clipboard(const char* result){

    if (result != NULL){
        GError *error = NULL;

        const gchar *const argv[] = {"/usr/bin/xclip", "-selection", "clipboard", NULL};
        GSubprocess *process = g_subprocess_newv(argv,
                                                 G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                                 G_SUBPROCESS_FLAGS_STDERR_MERGE, &error);

        if (error != NULL) {
            g_error("Spawning child failed: %s", error->message);
            g_error_free(error);
        }

        GOutputStream *output_stream = g_subprocess_get_stdin_pipe(process);
        g_output_stream_write_all(output_stream, result,
                                  strlen(result), NULL, NULL, NULL
        );

        g_output_stream_close(output_stream, NULL, NULL);
    }
}


static ModeMode calc_mode_result(Mode* sw, int menu_entry, G_GNUC_UNUSED char** input, unsigned int selected_line)
{
    ModeMode retv = MODE_EXIT;
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);
    if (menu_entry & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (menu_entry & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if (menu_entry & MENU_QUICK_SWITCH) {
        retv = (menu_entry & MENU_LOWER_MASK);
    } else if ((menu_entry & MENU_OK) && selected_line == 0) {
        if (!is_error_string(pd->last_result) && strlen(pd->last_result) > 0) {
            const char* result = get_only_result_part((const char **)&pd->last_result);
            copy_only_result_to_clipboard(result);
        }
        retv = MODE_EXIT;
    } else if (((menu_entry & MENU_OK) && selected_line == 1) ||
                ((menu_entry & MENU_CUSTOM_INPUT) && selected_line == -1u)) {
        if (!is_error_string(pd->last_result) && strlen(pd->last_result) > 0) {
            char* history_entry = g_strdup_printf("%s", pd->last_result);
            g_ptr_array_add(pd->history, (gpointer) history_entry);
        }
        retv = RESET_DIALOG;
    } else if ((menu_entry & MENU_OK) && selected_line > 1) {
        const char* result = get_only_result_part((const char **)&g_ptr_array_index(pd->history, get_real_history_index(pd->history, selected_line)));
        copy_only_result_to_clipboard(result);

        retv = MODE_EXIT;
    } else if (menu_entry & MENU_ENTRY_DELETE) {
        if (selected_line > 0) {
            g_ptr_array_remove_index(pd->history, get_real_history_index(pd->history, selected_line));
        }
        retv = RELOAD_DIALOG;
    }

    g_debug("selected_line: %i", selected_line);
    g_debug("ding: %x", menu_entry);
    g_debug("MENU_OK: %x", menu_entry & MENU_OK);
    g_debug("MENU_CANCEL: %x", menu_entry & MENU_CANCEL);
    g_debug("MENU_NEXT: %x", menu_entry & MENU_NEXT);
    g_debug("MENU_CUSTOM_INPUT: %x", menu_entry & MENU_CUSTOM_INPUT);
    g_debug("MENU_ENTRY_DELETE: %x", menu_entry & MENU_ENTRY_DELETE);
    g_debug("MENU_QUICK_SWITCH: %x", menu_entry & MENU_QUICK_SWITCH);
    g_debug("MENU_PREVIOUS: %x", menu_entry & MENU_PREVIOUS);
    g_debug("MENU_CUSTOM_ACTION: %x", menu_entry & MENU_CUSTOM_ACTION);
    g_debug("MENU_LOWER_MASK: %x", menu_entry & MENU_LOWER_MASK);
    return retv;
}

static void calc_mode_destroy(Mode* sw)
{
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);
    if (pd != NULL) {
        g_free(pd);
        mode_set_private_data(sw, NULL);
    }
}

static char* calc_get_display_value(const Mode* sw, unsigned int selected_line, G_GNUC_UNUSED int* state, G_GNUC_UNUSED GList** attr_list, int get_entry)
{
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);

    if (!get_entry) {
        return NULL;
    }

    if (selected_line == 0) {
        return g_strdup("Copy to clipboard and exit");
    }else if (selected_line == 1) {
        return g_strdup("Add to history");
    }
    unsigned int real_index = get_real_history_index(pd->history, selected_line);
    return g_strdup(g_ptr_array_index(pd->history, real_index));
}

static int calc_token_match(G_GNUC_UNUSED const Mode* sw, G_GNUC_UNUSED rofi_int_matcher** tokens, G_GNUC_UNUSED unsigned int index)
{
    return TRUE;
}

// It's a hacky way of making rofi show new window titles.
extern void rofi_view_reload(void);

static void process_cb(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    GError *error = NULL;
    GSubprocess* process = (GSubprocess*)source_object;
    GInputStream* stdout_stream = g_subprocess_get_stdout_pipe(process);
    char** last_result = (char**)user_data;

    g_subprocess_wait_check_finish(process, res, &error);

    if (error != NULL) {
        g_error("Process errored with: %s", error->message);
        g_error_free(error);
    }

    unsigned int stdout_bufsize = 4096;
    char stdout_buf[stdout_bufsize];
    g_input_stream_read_all(stdout_stream, stdout_buf, stdout_bufsize, NULL, NULL, &error);

    if (error != NULL) {
        g_error("Process errored with: %s", error->message);
        g_error_free(error);
    }

    // Check if there is an 'ans' variable
    const char* result = NULL;
    int ans = strncmp(stdout_buf, "> ans:=", 7);
    if (ans != 0){
        // No 'ans' variable, continue to proceed normally
        result = (const char *)stdout_buf;
    }else{
        // 'ans' variable, go until result
        result = strstr(stdout_buf, "\n\n>")+3;
    }

    // Skip the part with the question and go to the answer
    result = strstr(result, "\n\n  ")+4;

    unsigned int line_length = strcspn(result, "\n");
    *last_result = g_strndup(result, line_length);
    rofi_view_reload();
}

static char* calc_preprocess_input(Mode* sw, const char* input)
{
    GError *error = NULL;
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);

    const gchar* const argv[] = { "/usr/bin/qalc", "+u8", "-nocurrencies", NULL };
    GSubprocess* process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_MERGE | G_SUBPROCESS_FLAGS_STDIN_PIPE, &error);

    if (error != NULL) {
        g_error("Spawning child failed: %s", error->message);
        g_error_free(error);
    }

    GOutputStream * stdin_stream = g_subprocess_get_stdin_pipe(process);

    if (pd->history->len > 0) {
        // Add 'ans' variable with the last history
        char *cmd = g_strdup_printf("ans:=%s\n", get_only_result_part(
            (const char **) &g_ptr_array_index(pd->history, pd->history->len - 1)));
        g_output_stream_write(stdin_stream, (const void *) cmd, strlen(cmd), NULL, NULL);
    }

    // Send input to qalc
    g_output_stream_write (stdin_stream, (const void *)input, strlen(input), NULL, NULL);
    g_output_stream_close (stdin_stream, NULL, NULL);

    g_subprocess_wait_check_async(process, NULL, process_cb, (gpointer)&pd->last_result);

    return g_strdup(input);
}

static char *calc_get_message ( const Mode *sw )
{
    CALCModePrivateData* pd = (CALCModePrivateData*)mode_get_private_data(sw);
    if (is_error_string(pd->last_result)) {
        return g_markup_printf_escaped("<span foreground='PaleVioletRed'>%s</span>", pd->last_result);
    }
    return g_markup_printf_escaped("Result: <b>%s</b>\n<b>Ctrl+Enter</b>To add to history", pd->last_result);
}

Mode mode =
{
    .abi_version        = ABI_VERSION,
    .name               = "calc",
    .cfg_name_key       = "display-calc",
    ._init              = calc_mode_init,
    ._get_num_entries   = calc_mode_get_num_entries,
    ._result            = calc_mode_result,
    ._destroy           = calc_mode_destroy,
    ._token_match       = calc_token_match,
    ._get_display_value = calc_get_display_value,
    ._get_message       = calc_get_message,
    ._preprocess_input  = calc_preprocess_input,
    .private_data       = NULL,
    .free               = NULL,
};
