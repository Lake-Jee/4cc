/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 19.08.2015
 *
 * File editing view for 4coder
 *
 */

// TOP

enum Endline_Mode{
	ENDLINE_RN_COMBINED,
	ENDLINE_RN_SEPARATE,
	ENDLINE_RN_SHOWALLR
};

struct Range{
    i32 start, end;
};

enum Edit_Type{
    ED_NORMAL,
    ED_REVERSE_NORMAL,
    ED_UNDO,
    ED_REDO
};

struct Edit_Step{
    i32 str_start;
    i32 str_len;
    Range range;
    i32 pre_pos;
    i32 post_pos;
    bool32 can_merge;
    Edit_Type reverse_type;
    i32 next_block, prev_block;
};

struct Undo_Data{
    u8 *strings;
    i32 str_size, str_redo, str_max;
    
    i32 edit_count, edit_redo, edit_max;
    Edit_Step *edits;
    
    i32 str_history, str_history_max;
    u8 *history_strings;
    
    i32 history_block_count, history_head_block;
    Edit_Step *history;
    i32 edit_history, edit_history_max;
    i32 edit_history_cursor;
    i32 pending_block;
};

struct Editing_File{
    i32 size, max_size;
    u8 *data;
    
    Undo_Data undo;
    
    i32 line_count, line_max;
    i32 *line_starts;
    
    Font *font;
    i32 width_count, width_max;
    real32 *line_width;
    
    Endline_Mode endline_mode;
    i32 cursor_pos;
    bool32 is_dummy;
    
    char source_path_[256];
    char live_name_[256];
    char extension_[16];
    String source_path;
    String live_name;
    String extension;
    
    Cpp_Token_Stack token_stack;
    bool32 tokens_complete;
    bool32 tokens_exist;
    bool32 still_lexing;
    u32 lex_job;
    
    u64 last_4ed_write_time;
    u64 last_4ed_edit_time;
    u64 last_sys_write_time;
};

struct File_Table_Entry{
    String name;
    u32 hash;
    i32 index;
};

struct File_Table{
    File_Table_Entry *table;
    i32 count, max;
};

internal u32
get_file_hash(String name){
    u32 x = 5381;
    int i = 0;
    char c;
    while (i < name.size){
        c = name.str[i++];
        x = ((x << 5) + x) + c;
    }
    return x;
}

internal bool32
table_add(File_Table *table, String name, i32 index){
    Assert(table->count * 3 < table->max * 2);
    
    File_Table_Entry entry, e;
    i32 i;
    
    entry.name = name;
    entry.index = index;
    entry.hash = get_file_hash(name);
    i = entry.hash % table->max;
    while ((e = table->table[i]).name.str){
        if (e.hash == entry.hash && match(e.name, entry.name)){
            return 1;
        }
        i = (i + 1) % table->max;
    }
    table->table[i] = entry;
    ++table->count;
    
    return 0;
}

internal bool32
table_find_pos(File_Table *table, String name, i32 *index){
    File_Table_Entry e;
    i32 i;
    u32 hash;

    hash = get_file_hash(name);
    i = hash % table->max;
    while ((e = table->table[i]).name.size){
        if (e.name.str && e.hash == hash && match(e.name, name)){
            *index = i;
            return 1;
        }
        i = (i + 1) % table->max;
    }
    
    return 0;
}

inline bool32
table_find(File_Table *table, String name, i32 *index){
    i32 pos;
    bool32 r = table_find_pos(table, name, &pos);
    if (r) *index = table->table[pos].index;
    return r;
}

inline bool32
table_remove(File_Table *table, String name){
    i32 pos;
    bool32 r = table_find_pos(table, name, &pos);
    if (r){
        table->table[pos].name.str = 0;
        --table->count;
    }
    return r;
}

struct Working_Set{
	Editing_File *files;
	i32 file_index_count, file_max_count;
    
    File_Table table;
    
	String clipboards[64];
	i32 clipboard_size, clipboard_max_size;
	i32 clipboard_current, clipboard_rolling;
};

struct Text_Effect{
    i32 start, end;
    u32 color;
    i32 tick_down, tick_max;
};

struct View_Cursor_Data{
	i32 pos;
	i32 line, character;
    real32 unwrapped_x, unwrapped_y;
	real32 wrapped_x, wrapped_y;
};

struct File_View_Mode{
	bool8 rewrite;
};

struct Incremental_Search{
    String str;
    bool32 reverse;
    i32 pos;
};

enum Action_Type{
    DACT_OPEN,
    DACT_SAVE_AS,
    DACT_SAVE,
    DACT_NEW,
    DACT_SWITCH,
    DACT_TRY_KILL,
    DACT_KILL,
    DACT_CLOSE_MINOR,
    DACT_CLOSE_MAJOR,
    DACT_THEME_OPTIONS
};

struct Delayed_Action{
    Action_Type type;
    String string;
    Panel *panel;
};

struct Delay{
    Delayed_Action acts[8];
    i32 count, max;
};

inline void
delayed_action(Delay *delay, Action_Type type,
               String string, Panel *panel){
    Assert(delay->count < delay->max);
    Delayed_Action action;
    action.type = type;
    action.string = string;
    action.panel = panel;
    delay->acts[delay->count++] = action;
}

// Hot Directory

struct Hot_Directory{
	String string;
	File_List file_list;
};

internal void
hot_directory_init(Hot_Directory *hot_directory, String base){
	hot_directory->string = base;
    hot_directory->string.str[255] = 0;
	i32 dir_size = system_get_working_directory((u8*)hot_directory->string.str,
												hot_directory->string.memory_size);
	if (dir_size <= 0){
		dir_size = system_get_easy_directory((u8*)hot_directory->string.str);
	}
	hot_directory->string.size = dir_size;
	append(&hot_directory->string, "\\");
}

internal void
hot_directory_clean_end(Hot_Directory *hot_directory){
    String *str = &hot_directory->string;
    if (str->size != 0 && str->str[str->size-1] != '\\'){
        str->size = reverse_seek_slash(*str) + 1;
        str->str[str->size] = 0;
    }
}

internal i32
hot_directory_quick_partition(File_Info *infos, i32 start, i32 pivot){
    File_Info *p = infos + pivot;
    File_Info *a = infos + start;
    for (i32 i = start; i < pivot; ++i, ++a){
        i32 comp = 0;
        comp = p->folder - a->folder;
        if (comp == 0) comp = compare(a->filename, p->filename);
        if (comp < 0){
            Swap(*a, infos[start]);
            ++start;
        }
    }
    Swap(*p, infos[start]);
    return start;
}

internal void
hot_directory_quick_sort(File_Info *infos, i32 start, i32 pivot){
    i32 mid = hot_directory_quick_partition(infos, start, pivot);
    if (start < mid-1) hot_directory_quick_sort(infos, start, mid-1);
    if (mid+1 < pivot) hot_directory_quick_sort(infos, mid+1, pivot);
}

inline void
hot_directory_fixup(Hot_Directory *hot_directory, Working_Set *working_set){
    File_List *files = &hot_directory->file_list;
    if (files->count >= 2)
        hot_directory_quick_sort(files->infos, 0, files->count - 1);
}

inline void
hot_directory_set(Hot_Directory *hot_directory, String str, Working_Set *working_set){
    bool32 success = copy_checked(&hot_directory->string, str);
    terminate_with_null(&hot_directory->string);
    if (success){
		system_free_file_list(hot_directory->file_list);
		hot_directory->file_list = system_get_files(str);
    }
    hot_directory_fixup(hot_directory, working_set);
}

inline void
hot_directory_reload(Hot_Directory *hot_directory, Working_Set *working_set){
	if (hot_directory->file_list.block){
		system_free_file_list(hot_directory->file_list);
	}
	hot_directory->file_list = system_get_files(hot_directory->string);
    hot_directory_fixup(hot_directory, working_set);
}

struct Hot_Directory_Match{
	String filename;
	bool32 is_folder;
};

internal bool32
filename_match(String query, Absolutes *absolutes, String filename){
    bool32 result;
    result = (query.size == 0);
    if (!result) result = wildcard_match(absolutes, filename);
    return result;
}

internal Hot_Directory_Match
hot_directory_first_match(Hot_Directory *hot_directory,
                          String str,
						  bool32 include_files,
                          bool32 exact_match){
    Hot_Directory_Match result = {};
    
    Absolutes absolutes;
    if (!exact_match)
        get_absolutes(str, &absolutes, 1, 1);
    
    File_List *files = &hot_directory->file_list;
    File_Info *info, *end;
    end = files->infos + files->count;
    for (info = files->infos; info != end; ++info){
        String filename = info->filename;
        bool32 is_match = 0;
        if (exact_match){
            if (match(filename, str)) is_match = 1;
        }
        else{
            if (filename_match(str, &absolutes, filename)) is_match = 1;
        }
        
        if (is_match){
            result.is_folder = info->folder;
            result.filename = filename;
            break;
        }
    }
    
    return result;
}

struct Single_Line_Input_Step{
	bool32 hit_newline;
	bool32 hit_ctrl_newline;
	bool32 hit_a_character;
    bool32 hit_backspace;
	bool32 hit_esc;
	bool32 made_a_change;
    bool32 did_command;
};

enum Single_Line_Input_Type{
	SINGLE_LINE_STRING,
	SINGLE_LINE_FILE
};

struct Single_Line_Mode{
	Single_Line_Input_Type type;
	String *string;
	Hot_Directory *hot_directory;
    bool32 fast_folder_select;
};

internal Single_Line_Input_Step
app_single_line_input_core(Key_Codes *codes, Working_Set *working_set,
                           Key_Single key, Single_Line_Mode mode){
    Single_Line_Input_Step result = {};
    
    if (key.key.keycode == codes->back){
        result.hit_backspace = 1;
        if (mode.string->size > 0){
            result.made_a_change = 1;
            --mode.string->size;
            switch (mode.type){
            case SINGLE_LINE_STRING:
                mode.string->str[mode.string->size] = 0; break;
            
            case SINGLE_LINE_FILE:
            {
                char end_character = mode.string->str[mode.string->size];
                if (char_is_slash(end_character)){
                    mode.string->size = reverse_seek_slash(*mode.string) + 1;
                    mode.string->str[mode.string->size] = 0;
                    hot_directory_set(mode.hot_directory, *mode.string, working_set);
                }
                else{
                    mode.string->str[mode.string->size] = 0;
                }
            }break;
            }
        }
    }
    
    else if (key.key.character == '\n' || key.key.character == '\t'){
        result.made_a_change = 1;
        if (key.modifiers[CONTROL_KEY_CONTROL] ||
            key.modifiers[CONTROL_KEY_ALT]){
            result.hit_ctrl_newline = 1;
        }
        else{
            result.hit_newline = 1;
            if (mode.fast_folder_select){
                char front_name_space[256];
                String front_name =
                    make_string(front_name_space, 0, ArrayCount(front_name_space));
                get_front_of_directory(&front_name, *mode.string);
                Hot_Directory_Match match;
                match = hot_directory_first_match(mode.hot_directory, front_name, 1, 1);
                if (!match.filename.str){
                    match = hot_directory_first_match(mode.hot_directory, front_name, 1, 0);
                }
                if (match.filename.str){
                    if (match.is_folder){
                        set_last_folder(mode.string, match.filename);
                        hot_directory_set(mode.hot_directory, *mode.string, working_set);
                        result.hit_newline = 0;
                    }
                    else{
                        mode.string->size = reverse_seek_slash(*mode.string) + 1;
                        append(mode.string, match.filename);
                        result.hit_newline = 1;
                    }
                }
            }
        }
    }
    
    else if (key.key.keycode == codes->esc){
        result.hit_esc = 1;
        result.made_a_change = 1;
    }
    
    else if (key.key.character){
        result.hit_a_character = 1;
        if (!key.modifiers[CONTROL_KEY_CONTROL] &&
            !key.modifiers[CONTROL_KEY_ALT]){
            if (mode.string->size+1 < mode.string->memory_size){
                u8 new_character = (u8)key.key.character;
                mode.string->str[mode.string->size] = new_character;
                mode.string->size++;
                mode.string->str[mode.string->size] = 0;
                if (mode.type == SINGLE_LINE_FILE && char_is_slash(new_character)){
                    hot_directory_set(mode.hot_directory, *mode.string, working_set);
                }
                result.made_a_change = 1;
            }
        }
        else{
            result.did_command = 1;
            result.made_a_change = 1;
        }
    }
    
    return result;
}

inline Single_Line_Input_Step
app_single_line_input_step(Key_Codes *codes, Key_Single key, String *string){
	Single_Line_Mode mode = {};
	mode.type = SINGLE_LINE_STRING;
	mode.string = string;
	return app_single_line_input_core(codes, 0, key, mode);
}

inline Single_Line_Input_Step
app_single_file_input_step(Key_Codes *codes, Working_Set *working_set, Key_Single key,
						   String *string, Hot_Directory *hot_directory,
                           bool32 fast_folder_select){
	Single_Line_Mode mode = {};
	mode.type = SINGLE_LINE_FILE;
	mode.string = string;
	mode.hot_directory = hot_directory;
    mode.fast_folder_select = fast_folder_select;
	return app_single_line_input_core(codes, working_set, key, mode);
}

inline Single_Line_Input_Step
app_single_number_input_step(Key_Codes *codes, Key_Single key, String *string){
    Single_Line_Input_Step result = {};
	Single_Line_Mode mode = {};
	mode.type = SINGLE_LINE_STRING;
	mode.string = string;
    
    char c = (char)key.key.character;
    if (c == 0 || c == '\n' || char_is_numeric(c))
        result = app_single_line_input_core(codes, 0, key, mode);
	return result;
}

struct Widget_ID{
    i32 id;
    i32 sub_id0;
    i32 sub_id1;
    i32 sub_id2;
};

inline bool32
widget_match(Widget_ID s1, Widget_ID s2){
    return (s1.id == s2.id && s1.sub_id0 == s2.sub_id0 &&
            s1.sub_id1 == s2.sub_id1 && s1.sub_id2 == s2.sub_id2);
}

struct UI_State{
    Render_Target *target;
    Style *style;
    Font *font;
    Mouse_Summary *mouse;
    Key_Summary *keys;
    Key_Codes *codes;
    Working_Set *working_set;
    
    Widget_ID selected, hover, hot;
    bool32 activate_me;
    bool32 redraw;
    bool32 input_stage;
    i32 sub_id1_change;
    
    real32 height, view_y;
};

inline Widget_ID
make_id(UI_State *state, i32 id){
    Widget_ID r = state->selected;
    r.id = id;
    return r;
}

inline Widget_ID
make_sub0(UI_State *state, i32 id){
    Widget_ID r = state->selected;
    r.sub_id0 = id;
    return r;
}

inline Widget_ID
make_sub1(UI_State *state, i32 id){
    Widget_ID r = state->selected;
    r.sub_id1 = id;
    return r;
}

inline Widget_ID
make_sub2(UI_State *state, i32 id){
    Widget_ID r = state->selected;
    r.sub_id2 = id;
    return r;
}

inline bool32
is_selected(UI_State *state, Widget_ID id){
    return widget_match(state->selected, id);
}

inline bool32
is_hot(UI_State *state, Widget_ID id){
    return widget_match(state->hot, id);
}

inline bool32
is_hover(UI_State *state, Widget_ID id){
    return widget_match(state->hover, id);
}

struct UI_Layout{
    i32 row_count;
    i32 row_item_width;
    i32 row_max_item_height;
    
    i32_Rect rect;
    i32 x, y;    
};

struct UI_Layout_Restore{
    UI_Layout layout;
    UI_Layout *dest;
};

inline void
begin_layout(UI_Layout *layout, i32_Rect rect){
    layout->rect = rect;
    layout->x = rect.x0;
    layout->y = rect.y0;
    layout->row_count = 0;
    layout->row_max_item_height = 0;
}

inline void
begin_row(UI_Layout *layout, i32 count){
    layout->row_count = count;
    layout->row_item_width = (layout->rect.x1 - layout->x) / count; 
}

inline i32_Rect
layout_rect(UI_Layout *layout, i32 height){
    i32_Rect rect;
    rect.x0 = layout->x;
    rect.y0 = layout->y;
    rect.x1 = rect.x0;
    rect.y1 = rect.y0 + height;
    if (layout->row_count > 0){
        --layout->row_count;
        rect.x1 = rect.x0 + layout->row_item_width;
        layout->x += layout->row_item_width;
        layout->row_max_item_height = Max(height, layout->row_max_item_height);
    }
    if (layout->row_count == 0){
        rect.x1 = layout->rect.x1;
        layout->row_max_item_height = Max(height, layout->row_max_item_height);
        layout->y += layout->row_max_item_height;
        layout->x = layout->rect.x0;
        layout->row_max_item_height = 0;
    }
    return rect;
}

inline UI_Layout_Restore
begin_sub_layout(UI_Layout *layout, i32_Rect area){
    UI_Layout_Restore restore;
    restore.layout = *layout;
    restore.dest = layout;
    begin_layout(layout, area);
    return restore;
}

inline void
end_sub_layout(UI_Layout_Restore restore){
    *restore.dest = restore.layout;
}

struct UI_Style{
    u32 dark, dim, bright;
    u32 base, pop1, pop2;
};

internal UI_Style
get_ui_style(Style *style){
    UI_Style ui_style;
    ui_style.dark = style->main.back_color;
    ui_style.dim = style->main.margin_color;
    ui_style.bright = style->main.margin_active_color;
    ui_style.base = style->main.default_color;
    ui_style.pop1 = style->main.file_info_style.pop1_color;
    ui_style.pop2 = style->main.file_info_style.pop2_color;
    return ui_style;
}

internal UI_Style
get_ui_style_upper(Style *style){
    UI_Style ui_style;
    ui_style.dark = style->main.margin_color;
    ui_style.dim = style->main.margin_hover_color;
    ui_style.bright = style->main.margin_active_color;
    ui_style.base = style->main.default_color;
    ui_style.pop1 = style->main.file_info_style.pop1_color;
    ui_style.pop2 = style->main.file_info_style.pop2_color;
    return ui_style;
}

inline void
get_colors(UI_State *state, u32 *back, u32 *fore, Widget_ID wid, UI_Style style){
    bool32 hover = is_hover(state, wid);
    bool32 hot = is_hot(state, wid);
    i32 level = hot + hover;
    switch (level){
    case 2:
        *back = style.bright;
        *fore = style.dark;
        break;
    case 1:
        *back = style.dim;
        *fore = style.bright;
        break;
    case 0:
        *back = style.dark;
        *fore = style.bright;
        break;
    }
}

inline void
get_pop_color(UI_State *state, u32 *pop, Widget_ID wid, UI_Style style){
    bool32 hover = is_hover(state, wid);
    bool32 hot = is_hot(state, wid);
    i32 level = hot + hover;
    switch (level){
    case 2:
        *pop = style.pop1;
        break;
    case 1:
        *pop = style.pop1;
        break;
    case 0:
        *pop = style.pop1;
        break;
    }
}

internal UI_State
ui_state_init(UI_State *state_in, Render_Target *target, Input_Summary *user_input,
              Style *style, Working_Set *working_set, bool32 input_stage){
    UI_State state = {};
    state.target = target;
    state.style = style;
    state.font = style->font;
    state.working_set = working_set;
    if (user_input){
        state.mouse = &user_input->mouse;
        state.keys = &user_input->keys;
        state.codes = user_input->codes;
    }
    state.selected = state_in->selected;
    state.hot = state_in->hot;
    if (input_stage) state.hover = {};
    else state.hover = state_in->hover;
    state.redraw = 0;
    state.activate_me = 0;
    state.input_stage = input_stage;
    state.height = state_in->height;
    state.view_y = state_in->view_y;
    return state;
}

inline bool32
ui_state_match(UI_State a, UI_State b){
    return (widget_match(a.selected, b.selected) &&
            widget_match(a.hot, b.hot) &&
            widget_match(a.hover, b.hover));
}

internal bool32
ui_finish_frame(UI_State *persist_state, UI_State *state, UI_Layout *layout, i32_Rect rect,
                bool32 do_wheel, bool32 *did_activation){
    bool32 result = 0;
    real32 h = layout->y + persist_state->view_y - rect.y0;
    real32 max_y = h - (rect.y1 - rect.y0);
    
    persist_state->height = h;
    persist_state->view_y = state->view_y;
    
    if (state->input_stage){
        Mouse_Summary *mouse = state->mouse;
        if (mouse->wheel_used && do_wheel){
            persist_state->view_y += mouse->wheel_amount*state->font->height;
            result = 1;
        }
        if (mouse->release_l && widget_match(state->hot, state->hover)){
            if (did_activation) *did_activation = 1;
            if (state->activate_me){
                state->selected = state->hot;
            }
        }
        if (!mouse->l && !mouse->r){
            state->hot = {};
        }
        
        if (!ui_state_match(*persist_state, *state) || state->redraw){
            result = 1;
        }
        
        *persist_state = *state;
    }
    
    if (persist_state->view_y >= max_y) persist_state->view_y = max_y;
    if (persist_state->view_y < 0) persist_state->view_y = 0;

    return result;
}

internal bool32
ui_do_button_input(UI_State *state, i32_Rect rect, Widget_ID id, bool32 activate, bool32 *right = 0){
    bool32 result = 0;
    Mouse_Summary *mouse = state->mouse;
    bool32 hover = hit_check(mouse->mx, mouse->my, rect);
    if (hover){
        state->hover = id;
        if (activate) state->activate_me = 1;
        if (mouse->press_l || (mouse->press_r && right)) state->hot = id;
        if (mouse->l && mouse->r) state->hot = {};
    }
    bool32 is_match = is_hot(state, id);
    if (mouse->release_l && is_match){
        if (hover) result = 1;
        state->redraw = 1;
    }
    if (right && mouse->release_r && is_match){
        if (hover) *right = 1;
        state->redraw = 1;
    }
    return result;
}

internal bool32
ui_do_subdivided_button_input(UI_State *state, i32_Rect rect, i32 parts, Widget_ID id, bool32 activate, i32 *indx_out, bool32 *right = 0){
    bool32 result = 0;
    real32 x0, x1;
    i32_Rect sub_rect;
    Widget_ID sub_widg = id;
    real32 sub_width = (rect.x1 - rect.x0) / (real32)parts;
    sub_rect.y0 = rect.y0;
    sub_rect.y1 = rect.y1;
    x1 = (real32)rect.x0;
    
    for (i32 i = 0; i < parts; ++i){
        x0 = x1;
        x1 = x1 + sub_width;
        sub_rect.x0 = TRUNC32(x0);
        sub_rect.x1 = TRUNC32(x1);
        sub_widg.sub_id2 = i;
        if (ui_do_button_input(state, sub_rect, sub_widg, activate, right)){
            *indx_out = i;
            break;
        }
    }
    
    return result;
}

internal real32
ui_do_vscroll_input(UI_State *state, i32_Rect top, i32_Rect bottom, i32_Rect slider,
                    Widget_ID id, real32 val, real32 step_amount,
                    real32 smin, real32 smax, real32 vmin, real32 vmax){
    Mouse_Summary *mouse = state->mouse;
    i32 mx = mouse->mx;
    i32 my = mouse->my;
    if (hit_check(mx, my, top)){
        state->hover = id;
        state->hover.sub_id2 = 1;
    }
    if (hit_check(mx, my, bottom)){
        state->hover = id;
        state->hover.sub_id2 = 2;
    }
    if (hit_check(mx, my, slider)){
        state->hover = id;
        state->hover.sub_id2 = 3;
    }
    if (mouse->press_l) state->hot = state->hover;
    if (id.id == state->hot.id){
        if (mouse->release_l){
            Widget_ID wid1, wid2;
            wid1 = wid2 = id;
            wid1.sub_id2 = 1;
            wid2.sub_id2 = 2;
            if (state->hot.sub_id2 == 1 && is_hover(state, wid1)) val -= step_amount;
            if (state->hot.sub_id2 == 2 && is_hover(state, wid2)) val += step_amount;
            state->redraw = 1;
        }
        if (state->hot.sub_id2 == 3){
            real32 S, L;
            S = (real32)mouse->my - (slider.y1 - slider.y0) / 2;
            if (S < smin) S = smin;
            if (S > smax) S = smax;
            L = unlerp(smin, S, smax);
            val = lerp(vmin, L, vmax);
            state->redraw = 1;
        }
    }
    return val;
}

internal bool32
ui_do_text_field_input(UI_State *state, String *str){
    bool32 result = 0;
    Key_Summary *keys = state->keys;
    for (i32 key_i = 0; key_i < keys->count; ++key_i){
        Key_Single key = get_single_key(keys, key_i);
        char c = (char)key.key.character;
        if (char_is_basic(c) && str->size < str->memory_size-1){
            str->str[str->size++] = c;
            str->str[str->size] = 0;
        }
        else if (c == '\n'){
            result = 1;
        }
        else if (key.key.keycode == state->codes->back && str->size > 0){
            str->str[--str->size] = 0;
        }
    }
    return result;
}

internal bool32
ui_do_file_field_input(UI_State *state, Hot_Directory *hot_dir){
    bool32 result = 0;
    Key_Summary *keys = state->keys;
    for (i32 key_i = 0; key_i < keys->count; ++key_i){
        Key_Single key = get_single_key(keys, key_i);
        String *str = &hot_dir->string;
        terminate_with_null(str);
        Single_Line_Input_Step step =
            app_single_file_input_step(state->codes, state->working_set, key, str, hot_dir, 1);
        if (step.hit_newline || step.hit_ctrl_newline) result = 1;
    }
    return result;
}

internal bool32
ui_do_line_field_input(UI_State *state, String *string){
    bool32 result = 0;
    Key_Summary *keys = state->keys;
    for (i32 key_i = 0; key_i < keys->count; ++key_i){
        Key_Single key = get_single_key(keys, key_i);
        terminate_with_null(string);
        Single_Line_Input_Step step =
            app_single_line_input_step(state->codes, key, string);
        if (step.hit_newline || step.hit_ctrl_newline) result = 1;
    }
    return result;
}

internal bool32
ui_do_slider_input(UI_State *state, i32_Rect rect, Widget_ID wid,
                   real32 min, real32 max, real32 *v){
    bool32 result = 0;
    ui_do_button_input(state, rect, wid, 0);
    Mouse_Summary *mouse = state->mouse;
    if (is_hot(state, wid)){
        result = 1;
        *v = unlerp(min, (real32)mouse->mx, max);
        state->redraw = 1;
    }
    return result;
}

internal bool32
do_text_field(Widget_ID wid, UI_State *state, UI_Layout *layout,
              String prompt, String dest){
    bool32 result = 0;
    Font *font = state->font;
    i32 character_h = font->height;

    i32_Rect rect = layout_rect(layout, character_h);
    
    if (state->input_stage){
        ui_do_button_input(state, rect, wid, 1);
        if (is_selected(state, wid)){
            if (ui_do_text_field_input(state, &dest)){
                result = 1;
            }
        }
    }
    else{
        Render_Target *target = state->target;
        UI_Style ui_style = get_ui_style_upper(state->style);
        u32 back, fore, prompt_pop;
        get_colors(state, &back, &fore, wid, ui_style);
        get_pop_color(state, &prompt_pop, wid, ui_style);
        
        draw_rectangle(target, rect, back);
        i32 x = rect.x0;
        x = draw_string(target, font, prompt, x, rect.y0, prompt_pop);
        draw_string(target, font, dest, x, rect.y0, ui_style.base);
    }

    return result;
}

enum File_View_Widget_Type{
    FWIDG_NONE,
    FWIDG_SEARCH,
    FWIDG_GOTO_LINE,
    FWIDG_UNDOING,
    FWIDG_RESTORING,
    // never below this
    FWIDG_TYPE_COUNT
};

struct File_View_Widget{
    File_View_Widget_Type type;
    i32 height;
    UI_State state;
};

struct File_View{
    View view_base;

    Delay *delay;
    Editing_Layout *layout;
    
    Editing_File *file;
    Style *style;
    
    i32 font_advance;
    i32 font_height;
    
    View_Cursor_Data cursor;
    i32 mark;
    real32 scroll_y, target_y, vel_y;
    real32 scroll_x, target_x, vel_x;
    real32 preferred_x;
    View_Cursor_Data scroll_y_cursor;
    union{
        Incremental_Search isearch;
        struct{
            String str;
        } gotoline;
    };
    
    View_Cursor_Data temp_highlight;
    i32 temp_highlight_end_pos;
    bool32 show_temp_highlight;
    
    File_View_Mode mode, next_mode;
    File_View_Widget widget;
    real32 rewind_amount, rewind_speed;
    i32 rewind_max, scrub_max;
    bool32 unwrapped_lines;
    bool32 show_whitespace;
    
    i32 line_count, line_max;
    real32 *line_wrap_y;
    
    Text_Effect paste_effect;
};

inline File_View*
view_to_file_view(View *view){
    File_View* result = 0;
    if (view && view->type == VIEW_TYPE_FILE){
        result = (File_View*)view;
    }
    return result;
}

internal Range
get_range(i32 a, i32 b){
	Range result = {};
	if (a < b){
		result.start = a;
		result.end = b;
	}
	else{
		result.start = b;
		result.end = a;
	}
	return result;
}

inline i32
pos_adjust_to_right(i32 pos, u8 *data, i32 size){
	if (pos+1 < size &&
		data[pos] == '\r' &&
		data[pos+1] == '\n'){
		++pos;
	}
	return pos;
}

inline i32
pos_adjust_to_self(i32 pos, u8 *data, i32 size){
	if (pos > 0 &&
		data[pos-1] == '\r' &&
		data[pos] == '\n'){
		--pos;
	}
	return pos;
}

inline i32
pos_universal_fix(i32 pos, u8 *data, i32 size, Endline_Mode mode){
	if (mode == ENDLINE_RN_COMBINED)
		pos = pos_adjust_to_self(pos, data, size);
	return pos;
}

inline i32
starts_new_line(u8 character, Endline_Mode mode){
	return (character == '\n' || (mode == ENDLINE_RN_SEPARATE && character == '\r'));
}

inline void
buffer_init_strings(Editing_File *file){
    file->source_path = make_fixed_width_string(file->source_path_);
    file->live_name = make_fixed_width_string(file->live_name_);
    file->extension = make_fixed_width_string(file->extension_);
}

inline void
buffer_set_name(Editing_File *file, u8 *filename){
    String f, ext;
    f = make_string_slowly((char*)filename);
    copy_checked(&file->source_path, f);
    file->live_name = make_fixed_width_string(file->live_name_);
    get_front_of_directory(&file->live_name, f);
    ext = file_extension(f);
    copy(&file->extension, ext);
}

inline void
buffer_synchronize_times(Editing_File *file, u8 *filename){
    Time_Stamp stamp = system_file_time_stamp(filename);
    if (stamp.success){
        file->last_4ed_write_time = stamp.time;
        file->last_4ed_edit_time = stamp.time;
        file->last_sys_write_time = stamp.time;
    }
}

inline bool32
buffer_save(Editing_File *file, u8 *filename){
	bool32 result;
    result = system_save_file(filename, file->data, file->size);
    buffer_synchronize_times(file, filename);
    return result;
}

inline bool32
buffer_save_and_set_names(Editing_File *file, u8 *filename){
	bool32 result = 0;
	if (buffer_save(file, filename)){
		result = 1;
        buffer_set_name(file, filename);
	}
	return result;
}

internal i32
buffer_count_newlines(Editing_File *file, i32 start, i32 end){
    i32 count = 0;
    
    u8 *data = file->data;
    Endline_Mode end_mode = file->endline_mode;
    
    for (i32 i = start; i < end; ++i){
        bool32 new_line = 0;
        switch(data[i]){
        case '\n':
        {
            new_line = 1;
        }break;
        case '\r':
        {
            if (end_mode == ENDLINE_RN_SEPARATE){
                new_line = 1;
            }
        }break;
        }
        count += new_line;
    }
    
    return count;
}

internal bool32
buffer_check_newline(Endline_Mode end_mode, u8 character){
    bool32 result = 0;
    if (character == '\n' ||
        (end_mode == ENDLINE_RN_SEPARATE && character == '\r')){
        result = 1;
    }
    return result;
}

enum File_Bubble_Type{
    BUBBLE_BUFFER = 1,
    BUBBLE_STARTS,
    BUBBLE_WIDTHS,
    BUBBLE_WRAPS,
    BUBBLE_TOKENS,
    BUBBLE_UNDO_STRING,
    BUBBLE_UNDO,
    BUBBLE_HISTORY_STRING,
    BUBBLE_HISTORY,
    //
    FILE_BUBBLE_TYPE_END,
};

#define GROW_FAILED 0
#define GROW_NOT_NEEDED 1
#define GROW_SUCCESS 2

internal i32
buffer_grow_starts_as_needed(General_Memory *general, Editing_File *file, i32 additional_lines){
    bool32 result = GROW_NOT_NEEDED;
    i32 max = file->line_max;
    i32 count = file->line_count;
    i32 target_lines = count + additional_lines;
    if (target_lines > max){
        max <<= 1;
        i32 *new_lines = (i32*)general_memory_reallocate(general, file->line_starts,
                                                         sizeof(i32)*count, sizeof(i32)*max, BUBBLE_STARTS);
        if (new_lines){
            file->line_starts = new_lines;
            file->line_max = max;
            result = GROW_SUCCESS;
        }
        else{
            result = GROW_FAILED;
        }
    }
    return result;
}

internal void
buffer_measure_starts(General_Memory *general, Editing_File *file){
    ProfileMomentFunction();
    if (!file->line_starts){
        i32 max = file->line_max = Kbytes(1);
        file->line_starts = (i32*)general_memory_allocate(general, max*sizeof(i32), BUBBLE_STARTS);
        // TODO(allen): when unable to allocate?
    }
    Endline_Mode mode = file->endline_mode;
    
    i32 size = file->size;
    u8 *data = file->data;
    
    file->line_count = 0;
    i32 start = 0;
    for (i32 i = 0; i < size; ++i){
        if (buffer_check_newline(mode, data[i])){
            // TODO(allen): when unable to grow?
            buffer_grow_starts_as_needed(general, file, 1);
            file->line_starts[file->line_count++] = start;
            start = i + 1;
        }
    }
    
    // TODO(allen): when unable to grow?
    buffer_grow_starts_as_needed(general, file, 1);
    file->line_starts[file->line_count++] = start;
}

internal real32
measure_character(Font *font, Endline_Mode end_mode, bool32 *new_line,
                  u8 character, u8 next){
    real32 width = 0;
    switch (character){
    case 0:
    {
        width = 0;
        *new_line = 1;
    }break;
    
    case '\n':
    {
        width = font->chardata[character].xadvance;
        *new_line = 1;
    }break;
    
    case '\r':
    {
        switch (end_mode){
        case ENDLINE_RN_COMBINED:
        {
            if (next == '\n'){
                // DO NOTHING
            }
            else{
                width = font->chardata['\\'].xadvance;
                width += font->chardata['r'].xadvance;
            }
        }break;
        
        case ENDLINE_RN_SEPARATE:
        {
            width = font->chardata[character].xadvance;
            *new_line = 1;
        }break;
        
        case ENDLINE_RN_SHOWALLR:
        {
            width = font->chardata['\\'].xadvance;
            width += font->chardata['r'].xadvance;
        }break;
        }
    }break;
    
    default:
    {
        width = font->chardata[character].xadvance;
    }break;
    }
    return width;
}

internal void
buffer_measure_widths(General_Memory *general, Editing_File *file, Font *font){
    ProfileMomentFunction();
    i32 line_count = file->line_count;
    if (line_count > file->width_max){
        i32 new_max = LargeRoundUp(line_count, Kbytes(1));
        if (file->line_width){
            file->line_width = (real32*)
                general_memory_reallocate_nocopy(general, file->line_width, sizeof(real32)*new_max, BUBBLE_WIDTHS);
        }
        else{
            file->line_width = (real32*)
                general_memory_allocate(general, sizeof(real32)*new_max, BUBBLE_WIDTHS);
        }
        file->width_max = new_max;
    }
    
    Endline_Mode mode = file->endline_mode;
    i32 size = file->size;
    u8 *data = file->data;
    real32 *line_widths = file->line_width;
    
    // TODO(allen): Does this help at all with widths???
    // probably only if we go parallel...
    i32 *starts = file->line_starts; AllowLocal(starts);
    
    data[size] = 0;
    for (i32 i = 0, j = 0; i < line_count; ++i){
        TentativeAssert(j == starts[i]);
        bool32 new_line = 0;
        real32 width = 0;
        u8 c = data[j];
        u8 n = data[++j];
        while (new_line == 0){
            width += measure_character(font, mode, &new_line, c, n);
            c = n;
            n = data[++j];
        }
        --j;
        line_widths[i] = width;
    }
}

internal void
buffer_remeasure_starts(General_Memory *general, Editing_File *file,
                        i32 line_start, i32 line_end, i32 line_shift,
                        i32 character_shift){
    ProfileMomentFunction();
    Assert(file->line_starts);
    buffer_grow_starts_as_needed(general, file, line_shift);
    i32 *lines = file->line_starts;
    
    if (line_shift != 0){
        memmove(lines + line_end + line_shift + 1, lines + line_end + 1,
                sizeof(i32)*(file->line_count - line_end - 1));
        file->line_count += line_shift;
    }
    
    if (character_shift != 0){
        i32 line_count = file->line_count;
        i32 *line = lines + line_end + 1;
        for (i32 i = line_end + 1; i < line_count; ++i, ++line){
            *line += character_shift;
        }
    }
    
    i32 size = file->size;
    u8 *data = file->data;
    i32 char_i = lines[line_start];
    i32 line_i = line_start;
    
    Endline_Mode end_mode = file->endline_mode;
    
    i32 start = char_i;
    for (; char_i <= size; ++char_i){
        u8 character;
        if (char_i == size){
            character = '\n';
        }
        else{
            character = data[char_i];
        }
        
        if (buffer_check_newline(end_mode, character)){
            if (line_i > line_end && start == lines[line_i]){
                break;
            }
            
            lines[line_i++] = start;
            start = char_i + 1;
        }
    }
}

internal i32
buffer_get_line_index(Editing_File *file, i32 pos){
    i32 result;
    i32 start = 0;
    i32 end = file->line_count;
    i32 *lines = file->line_starts;
    while (1){
        i32 i = (start + end) / 2;
        if (lines[i] < pos){
            start = i;
        }
        else if (lines[i] > pos){
            end = i;
        }
        else{
            result = i;
            break;
        }
        Assert(start < end);
        if (start ==  end - 1){
            result = start;
            break;
        }
    }
    return result;
}

inline i32
view_wrapped_line_span(real32 line_width, real32 max_width){
    i32 line_count = CEIL32(line_width / max_width);
    if (line_count == 0) line_count = 1;
    return line_count;
}

internal i32
view_compute_lowest_line(File_View *view){
    i32 last_line = view->line_count - 1;
    i32 lowest_line = 0;
    if (last_line > 0){
        if (view->unwrapped_lines){
            lowest_line = last_line;
        }
        else{
            real32 wrap_y = view->line_wrap_y[last_line];
            Editing_File *file = view->file;
            AllowLocal(file);
            Assert(!file->is_dummy);
            Style *style = view->style;
            Font *font = style->font;
            lowest_line = FLOOR32(wrap_y / font->height);
            
            real32 width = file->line_width[last_line];
            real32 max_width = view_compute_width(view);
            i32 line_span = view_wrapped_line_span(width, max_width);
            lowest_line += line_span - 1;
        }
    }
    return lowest_line;
}

internal void
view_measure_wraps(General_Memory *general, File_View *view){
    ProfileMomentFunction();
    Editing_File *file = view->file;
    i32 line_count = file->line_count;
    
    if (view->line_max < line_count){
        i32 max = view->line_max = LargeRoundUp(line_count, Kbytes(1));
        if (view->line_wrap_y){
            view->line_wrap_y = (real32*)
                general_memory_reallocate_nocopy(general, view->line_wrap_y, sizeof(real32)*max, BUBBLE_WRAPS);
        }
        else{
            view->line_wrap_y = (real32*)
                general_memory_allocate(general, sizeof(real32)*max, BUBBLE_WRAPS);
        }
    }
    
    Font *font = view->style->font;
    real32 line_height = (real32)font->height;
    real32 max_width = view_compute_width(view);
    real32 *line_widths = file->line_width;
    real32 *line_wraps = view->line_wrap_y;
    i32 y_pos = 0;
    for (i32 i = 0; i < line_count; ++i){
        line_wraps[i] = y_pos*line_height;
        i32 line_span = view_wrapped_line_span(line_widths[i], max_width);
        y_pos += line_span;
    }
    
    view->line_count = line_count;
}

internal void
buffer_create_from_string(General_Memory *general, Editing_File *file, u8 *filename, Font *font, String val){
    i32 request_size = LargeRoundUp(1+val.size*2, Kbytes(64));
    u8 *data = (u8*)general_memory_allocate(general, request_size, BUBBLE_BUFFER);
    
    // TODO(allen): if we didn't get the memory what is going on?
    // request_size too large?
    // need to expand general memory allocator's base pool?
    // need to release least recently used target?
    Assert(data);
    
    *file = {};
    file->data = data;
    file->size = val.size;
    file->max_size = request_size;
    
    if (val.size > 0) memcpy(data, val.str, val.size);
    data[val.size] = 0;
    
    buffer_synchronize_times(file, filename);
    buffer_init_strings(file);
    buffer_set_name(file, filename);
    
    buffer_measure_starts(general, file);
    buffer_measure_widths(general, file, font);
    file->font = font;

    file->undo.str_max = request_size;
    file->undo.str_redo = file->undo.str_max;
    file->undo.strings = (u8*)general_memory_allocate(general, request_size, BUBBLE_UNDO_STRING);
    
    file->undo.edit_max = request_size / sizeof(Edit_Step);
    file->undo.edit_redo = file->undo.edit_max;
    file->undo.edits = (Edit_Step*)general_memory_allocate(general, request_size, BUBBLE_UNDO);
    
    file->undo.str_history_max = request_size;
    file->undo.history_strings = (u8*)general_memory_allocate(general, request_size, BUBBLE_HISTORY_STRING);
    
    file->undo.edit_history_max = request_size / sizeof(Edit_Step);
    file->undo.history = (Edit_Step*)general_memory_allocate(general, request_size, BUBBLE_HISTORY);
    
    file->undo.history_block_count = 1;
    file->undo.history_head_block = 0;
    file->undo.pending_block = 0;
    
    if (!match(file->extension, make_lit_string("txt"))){
        file->tokens_exist = 1;
    }
}

internal bool32
buffer_create(General_Memory *general, Editing_File *file, u8 *filename, Font *font){
    bool32 result = 0;
    
    File_Data raw_file = system_load_file(filename);
    if (raw_file.data){
        result = 1;
        String val = make_string((char*)raw_file.data, raw_file.size);
        buffer_create_from_string(general, file, filename, font, val);
        system_free_file(raw_file);
    }
    
    return result;
}

internal bool32
buffer_create_empty(General_Memory *general, Editing_File *file, u8 *filename, Font *font){
    bool32 result = 1;
    
    String empty_str = {};
    buffer_create_from_string(general, file, filename, font, empty_str);
    
    return result;
}

struct Get_File_Result{
    Editing_File *file;
    i32 index;
};

internal Get_File_Result
working_set_get_available_file(Working_Set *working_set){
    Get_File_Result result = {};
    
    for (i32 buffer_index = 1; buffer_index < working_set->file_max_count; ++buffer_index){
        if (buffer_index == working_set->file_index_count ||
            working_set->files[buffer_index].is_dummy){
            result.index = buffer_index;
            result.file = working_set->files + buffer_index;
            if (buffer_index == working_set->file_index_count){
                ++working_set->file_index_count;
            }
            break;
        }
    }
    
    Assert(result.file);
    return result;
}

internal void
buffer_close(General_Memory *general, Editing_File *file){
    if (file->still_lexing){
        system_cancel_job(BACKGROUND_THREADS, file->lex_job);
    }
    if (file->token_stack.tokens){
        general_memory_free(general, file->token_stack.tokens);
    }
    general_memory_free(general, file->data);
    general_memory_free(general, file->line_starts);
    general_memory_free(general, file->line_width);
}

internal void
buffer_get_dummy(Editing_File *buffer){
	*buffer = {};
	buffer->data = (u8*)&buffer->size;
	buffer->is_dummy = 1;
}

struct Shift_Information{
	i32 start, end, amount;
};

internal
JOB_CALLBACK(job_full_lex){
    Editing_File *file = (Editing_File*)data[0];
    General_Memory *general = (General_Memory*)data[1];
    
    Cpp_File cpp_file;
    cpp_file.data = (char*)file->data;
    cpp_file.size = file->size;
    
    Cpp_Token_Stack tokens;
    tokens.tokens = (Cpp_Token*)memory->data;
    tokens.max_count = memory->size / sizeof(Cpp_Token);
    tokens.count = 0;
    
    Cpp_Lex_Data status;
    status = cpp_lex_file_nonalloc(cpp_file, &tokens);
    while (!status.complete){
        system_grow_thread_memory(memory);
        tokens.tokens = (Cpp_Token*)memory->data;
        tokens.max_count = memory->size / sizeof(Cpp_Token);
        status = cpp_lex_file_nonalloc(cpp_file, &tokens, status);
    }
    
    i32 new_max = LargeRoundUp(tokens.count, Kbytes(1));
    system_aquire_lock(FRAME_LOCK);
    if (file->token_stack.tokens){
        file->token_stack.tokens = (Cpp_Token*)
            general_memory_reallocate_nocopy(general, file->token_stack.tokens, new_max*sizeof(Cpp_Token), BUBBLE_TOKENS);
    }
    else{
        file->token_stack.tokens = (Cpp_Token*)
            general_memory_allocate(general, new_max*sizeof(Cpp_Token), BUBBLE_TOKENS);
    }
    system_release_lock(FRAME_LOCK);
    
    i32 copy_amount = Kbytes(8);
    i32 uncoppied = tokens.count*sizeof(Cpp_Token);
    if (copy_amount > uncoppied) copy_amount = uncoppied;
    
    u8 *dest = (u8*)file->token_stack.tokens;
    u8 *src = (u8*)tokens.tokens;
    
    while (uncoppied > 0){
        system_aquire_lock(FRAME_LOCK);
        memcpy(dest, src, copy_amount);
        system_release_lock(FRAME_LOCK);
        dest += copy_amount;
        src += copy_amount;
        uncoppied -= copy_amount;
        if (copy_amount > uncoppied) copy_amount = uncoppied;
    }
    
    file->token_stack.count = tokens.count;
    file->token_stack.max_count = new_max;
    system_force_redraw();
    
    // NOTE(allen): These are outside the locked section because I don't
    // think getting these out of order will cause critical bugs, and I
    // want to minimize what's done in locked sections.
    file->tokens_complete = 1;
    file->still_lexing = 0;
}

internal void
buffer_kill_tokens(General_Memory *general, Editing_File *file){
    if (file->still_lexing){
        system_cancel_job(BACKGROUND_THREADS, file->lex_job);
    }
    if (file->token_stack.tokens){
        general_memory_free(general, file->token_stack.tokens);
    }
    file->tokens_exist = 0;
    file->tokens_complete = 0;
    file->token_stack = {};
}

internal void
buffer_first_lex_parallel(General_Memory *general, Editing_File *file){
    Assert(file->token_stack.tokens == 0);
    
    file->tokens_complete = 0;
    file->tokens_exist = 1;
    file->still_lexing = 1;
    
    Job_Data job;
    job.callback = job_full_lex;
    job.data[0] = file;
    job.data[1] = general;
    job.memory_request = Kbytes(64);
    file->lex_job = system_post_job(BACKGROUND_THREADS, job);
}

internal void
buffer_relex_parallel(Mem_Options *mem, Editing_File *file,
                      i32 start_i, i32 end_i, i32 amount){
    General_Memory *general = &mem->general;
    Partition *part = &mem->part;
    if (file->token_stack.tokens == 0){
        buffer_first_lex_parallel(general, file);
    }
    
    else{
        bool32 inline_lex = !file->still_lexing;
        if (inline_lex){
            Cpp_File cpp_file;
            cpp_file.data = (char*)file->data;
            cpp_file.size = file->size;
            
            Cpp_Token_Stack *stack = &file->token_stack;
            
            Cpp_Relex_State state = 
                cpp_relex_nonalloc_start(cpp_file, stack,
                                         start_i, end_i, amount, 100);
            
            Temp_Memory temp = begin_temp_memory(part);
            i32 relex_end;
            Cpp_Token_Stack relex_space;
            relex_space.count = 0;
            relex_space.max_count = state.space_request;
            relex_space.tokens = push_array(part, Cpp_Token, relex_space.max_count);
            if (cpp_relex_nonalloc_main(&state, &relex_space, &relex_end)){
                inline_lex = 0;
            }
            else{
                i32 delete_amount = relex_end - state.start_token_i;
                i32 shift_amount = relex_space.count - delete_amount;
                
                if (shift_amount != 0){
                    int new_count = stack->count + shift_amount;
                    if (new_count > stack->max_count){
                        int new_max = LargeRoundUp(new_count, Kbytes(1));
                        stack->tokens = (Cpp_Token*)
                            general_memory_reallocate(general, stack->tokens,
                                                      stack->count*sizeof(Cpp_Token),
                                                      new_max*sizeof(Cpp_Token), BUBBLE_TOKENS);
                        stack->max_count = new_max;
                    }
                    
                    int shift_size = stack->count - relex_end;
                    if (shift_size > 0){
                        Cpp_Token *old_base = stack->tokens + relex_end;
                        memmove(old_base + shift_amount, old_base,
                                sizeof(Cpp_Token)*shift_size);
                    }
                    
                    stack->count += shift_amount;
                }
                
                memcpy(state.stack->tokens + state.start_token_i, relex_space.tokens,
                       sizeof(Cpp_Token)*relex_space.count);
            }
            
            end_temp_memory(temp);
        }
        
        if (!inline_lex){
            i32 end_token_i = cpp_get_end_token(&file->token_stack, end_i);
            cpp_shift_token_starts(&file->token_stack, end_token_i, amount);
            --end_token_i;
            if (end_token_i >= 0){
                Cpp_Token *token = file->token_stack.tokens + end_token_i;
                if (token->start < end_i && token->start + token->size > end_i){
                    token->size += amount;
                }
            }
            
            file->still_lexing = 1;
            
            Job_Data job;
            job.callback = job_full_lex;
            job.data[0] = file;
            job.data[1] = general;
            job.memory_request = Kbytes(64);
            file->lex_job = system_post_job(BACKGROUND_THREADS, job);
        }
    }
}

internal bool32
buffer_grow_as_needed(General_Memory *general, Editing_File *file, i32 additional_size){
    bool32 result = 1;
    i32 target_size = file->size + additional_size + 1;
    if (target_size >= file->max_size){
		i32 request_size = LargeRoundUp(target_size*2, Kbytes(256));
        u8 *new_data = (u8*)general_memory_reallocate(general, file->data, file->size, request_size, BUBBLE_BUFFER);
		if (new_data){
            new_data[file->size] = 0;
			file->data = new_data;
			file->max_size = request_size;
		}
        else{
            result = 0;
        }
	}
    return result;
}

inline void
buffer_grow_undo_string(General_Memory *general, Editing_File *file, i32 extra_size){
    i32 old_max = file->undo.str_max;
    u8 *old_str = file->undo.strings;
    i32 redo_size = old_max - file->undo.str_redo;
    i32 new_max = old_max*2 + extra_size;
    u8 *new_str = (u8*)
        general_memory_allocate(general, new_max, BUBBLE_UNDO_STRING);
    i32 new_redo = new_max - redo_size;
    
    memcpy(new_str, old_str, file->undo.str_size);
    memcpy(new_str + new_redo, old_str + file->undo.str_redo, redo_size);
    
    general_memory_free(general, old_str);
    
    file->undo.strings = new_str;
    file->undo.str_max = new_max;
    file->undo.str_redo = new_redo;
}

inline void
buffer_grow_undo_edits(General_Memory *general, Editing_File *file){
    i32 old_max = file->undo.edit_max;
    Edit_Step *old_eds = file->undo.edits;
    i32 redo_size = old_max - file->undo.edit_redo;
    i32 new_max = old_max*2 + 2;
    Edit_Step *new_eds = (Edit_Step*)
        general_memory_allocate(general, new_max*sizeof(Edit_Step), BUBBLE_UNDO);
    i32 new_redo = new_max - redo_size;
    
    memcpy(new_eds, old_eds, file->undo.edit_count*sizeof(Edit_Step));
    memcpy(new_eds + new_redo, old_eds, redo_size*sizeof(Edit_Step));
    
    general_memory_free(general, old_eds);
    
    file->undo.edits = new_eds;
    file->undo.edit_max = new_max;
    file->undo.edit_redo = new_redo;
}

inline void
buffer_grow_history_string(General_Memory *general, Editing_File *file, i32 extra_size){
    i32 old_max = file->undo.str_max;
    u8 *old_str = file->undo.strings;
    i32 new_max = old_max*2 + extra_size;
    u8 *new_str = (u8*)
        general_memory_reallocate(general, old_str, old_max, new_max, BUBBLE_HISTORY_STRING);
    
    file->undo.history_strings = new_str;
    file->undo.str_history_max = new_max;
}

inline void
buffer_grow_history_edits(General_Memory *general, Editing_File *file){
    i32 old_max = file->undo.edit_max;
    Edit_Step *old_eds = file->undo.edits;
    i32 new_max = old_max*2 + 2;
    Edit_Step *new_eds = (Edit_Step*)
        general_memory_reallocate(general, old_eds, old_max*sizeof(Edit_Step),
                                  new_max*sizeof(Edit_Step), BUBBLE_UNDO);
    
    file->undo.history = new_eds;
    file->undo.edit_history_max = new_max;
}

internal Edit_Step*
buffer_post_undo(General_Memory *general, Editing_File *file,
                 i32 start, i32 end, i32 str_len, bool32 do_merge, bool32 can_merge,
                 bool32 clear_redo, i32 pre_pos = -1, i32 post_pos = -1){
    String replaced = make_string((char*)file->data + start, end - start);

    if (clear_redo){
        file->undo.str_redo = file->undo.str_max;
        file->undo.edit_redo = file->undo.edit_max;
    }
    
    if (file->undo.str_redo < file->undo.str_size + replaced.size)
        buffer_grow_undo_string(general, file, replaced.size);
    
    Edit_Step edit;
    edit.str_start = file->undo.str_size;
    edit.str_len = replaced.size;
    edit.range = {start, start + str_len};
    if (pre_pos != -1) edit.pre_pos = pre_pos;
    else edit.pre_pos = file->cursor_pos;
    if (post_pos != -1) edit.post_pos = post_pos;
    edit.can_merge = can_merge;
    edit.reverse_type = ED_REDO;
    
    file->undo.str_size += replaced.size;
    copy_fast_unsafe((char*)file->undo.strings + edit.str_start, replaced);
    
    bool32 did_merge = 0;
    if (do_merge && file->undo.edit_count > 0){
        Edit_Step prev = file->undo.edits[file->undo.edit_count-1];
        if (prev.can_merge && edit.str_len == 0 && prev.str_len == 0){
            if (prev.range.end == edit.range.start){
                did_merge = 1;
                edit.range.start = prev.range.start;
                edit.pre_pos = prev.pre_pos;
            }
        }
    }
    
    Edit_Step *result = 0;
    if (did_merge){
        result = file->undo.edits + (file->undo.edit_count-1);
        *result = edit;
    }
    else{
        if (file->undo.edit_redo <= file->undo.edit_count)
            buffer_grow_undo_edits(general, file);
        result = file->undo.edits + (file->undo.edit_count++);
        *result = edit;
    }
    
    return result;
}

inline void
buffer_unpost_undo(Editing_File *file){
    if (file->undo.edit_count > 0){
        Edit_Step *edit = file->undo.edits + (--file->undo.edit_count);
        file->undo.str_size -= edit->str_len;
    }
}

internal void
buffer_post_redo(General_Memory *general, Editing_File *file,
                 i32 start, i32 end, i32 str_len, i32 pre_pos, i32 post_pos){
    String replaced = make_string((char*)file->data + start, end - start);
    
    if (file->undo.str_redo - replaced.size < file->undo.str_size)
        buffer_grow_undo_string(general, file, replaced.size);
    
    file->undo.str_redo -= replaced.size;
    TentativeAssert(file->undo.str_redo >= file->undo.str_size);
    
    Edit_Step edit;
    edit.str_start = file->undo.str_redo;
    edit.str_len = replaced.size;
    edit.range = {start, start + str_len};
    edit.pre_pos = pre_pos;
    edit.post_pos = post_pos;
    edit.reverse_type = ED_UNDO;
    
    copy_fast_unsafe((char*)file->undo.strings + edit.str_start, replaced);
    
    if (file->undo.edit_redo <= file->undo.edit_count)
        buffer_grow_undo_edits(general, file);
    --file->undo.edit_redo;
    file->undo.edits[file->undo.edit_redo] = edit;
}

inline void
buffer_post_history_block(Editing_File *file, i32 pos){
    Assert(file->undo.history_head_block < pos);
    Assert(pos < file->undo.edit_history);
    
    Edit_Step *history = file->undo.history;
    Edit_Step *step = history + file->undo.history_head_block;
    step->next_block = pos;
    step = history + pos;
    step->prev_block = file->undo.history_head_block;
    file->undo.history_head_block = pos;
    ++file->undo.history_block_count;
}

inline void
buffer_unpost_history_block(Editing_File *file){
    Assert(file->undo.history_block_count > 1);
    
}

internal Edit_Step*
buffer_post_history(General_Memory *general, Editing_File *file,
                    i32 start, i32 end, i32 str_len,
                    bool32 do_merge, bool32 can_merge, Edit_Type reverse_type,
                    i32 pre_pos = -1, i32 post_pos = -1){
    String replaced = make_string((char*)file->data + start, end - start);
    
    if (file->undo.str_history_max < file->undo.str_history + replaced.size)
        buffer_grow_history_string(general, file, replaced.size);
    
    Edit_Step edit;
    edit.str_start = file->undo.str_history;
    edit.str_len = replaced.size;
    edit.range = {start, start + str_len};
    if (pre_pos != -1) edit.pre_pos = pre_pos;
    else edit.pre_pos = file->cursor_pos;
    if (post_pos != -1) edit.post_pos = post_pos;
    edit.can_merge = can_merge;
    edit.reverse_type = reverse_type;
    
    file->undo.str_history += replaced.size;
    copy_fast_unsafe((char*)file->undo.history_strings + edit.str_start, replaced);
    
    bool32 did_merge = 0;
    if (do_merge && file->undo.edit_history > 0){
        Edit_Step prev = file->undo.edits[file->undo.edit_count-1];
        if (prev.can_merge && edit.str_len == 0 && prev.str_len == 0){
            if (prev.range.end == edit.range.start && reverse_type == edit.reverse_type){
                did_merge = 1;
                edit.range.start = prev.range.start;
                edit.pre_pos = prev.pre_pos;
            }
        }
    }
    
    Edit_Step *result = 0;
    if (did_merge){
        result = file->undo.history + (file->undo.edit_history-1);
    }
    else{
        if (file->undo.edit_history_max <= file->undo.edit_history)
            buffer_grow_history_edits(general, file);
        result = file->undo.history + (file->undo.edit_history++);
    }
    
    *result = edit;
    
    return result;
}

inline i32
buffer_text_replace(General_Memory *general, Editing_File *file,
                    i32 start, i32 end, u8 *str, i32 str_len){
	i32 shift_amount = (str_len - (end - start));
    
    buffer_grow_as_needed(general, file, shift_amount);
	Assert(shift_amount + file->size + 1 <= file->max_size);
	
    i32 size = file->size;
    u8 *data = (u8*)file->data;
    memmove(data + end + shift_amount, data + end, size - end);
    file->size += shift_amount;
    file->data[file->size] = 0;
    
    memcpy(data + start, str, str_len);
    
    return shift_amount;
}

// TODO(allen): Proper strings?
internal Shift_Information
buffer_replace_range(Mem_Options *mem, Editing_File *file,
                     i32 start, i32 end, u8 *str, i32 str_len){
    ProfileMomentFunction();
    
    if (file->still_lexing)
        system_cancel_job(BACKGROUND_THREADS, file->lex_job);
    
    file->last_4ed_edit_time = system_get_now();

    General_Memory *general = &mem->general;
    i32 shift_amount = buffer_text_replace(general, file, start, end, str, str_len);
    
    if (file->tokens_exist)
        buffer_relex_parallel(mem, file, start, end, shift_amount);
    
    i32 line_start = buffer_get_line_index(file, start);
    i32 line_end = buffer_get_line_index(file, end);
    i32 replaced_line_count = line_end - line_start;
    i32 new_line_count = buffer_count_newlines(file, start, start+str_len);
    i32 line_shift =  new_line_count - replaced_line_count;
    
    buffer_remeasure_starts(general, file, line_start, line_end, line_shift, shift_amount);
    
    // TODO(allen): Can we "remeasure" widths now!?
    buffer_measure_widths(general, file, file->font);
    
    Shift_Information shift;
    shift.start = start;
    shift.end = end;
    shift.amount = shift_amount;
	return shift;
}

#if 0
inline internal void
buffer_delete(Mem_Options *mem, Editing_File *file, i32 pos){
	buffer_replace_range(mem, file, pos, pos+1, 0, 0);
}

inline internal void
buffer_delete_range(Mem_Options *mem, Editing_File *file, i32 smaller, i32 larger){
	buffer_replace_range(mem, file, smaller, larger, 0, 0);
}

inline internal void
buffer_delete_range(Mem_Options *mem, Editing_File *file, Range range){
	buffer_replace_range(mem, file, range.smaller, range.larger, 0, 0);
}
#endif

enum Endline_Convert_Type{
	ENDLINE_RN,
	ENDLINE_N,
	ENDLINE_R,
	ENDLINE_ERASE,
};

enum Panel_Seek_Type{
	SEEK_POS,
	SEEK_WRAPPED_XY,
	SEEK_UNWRAPPED_XY,
	SEEK_LINE_CHAR
};

struct Panel_Seek{
	Panel_Seek_Type type;
	union{
		struct{
			i32 pos;
		};
		struct{
			real32 x, y;
            bool32 round_down;
		};
		struct{
			i32 line, character;
		};
	};
};

inline internal Panel_Seek
seek_pos(i32 pos){
	Panel_Seek result = {};
	result.type = SEEK_POS;
	result.pos = pos;
	return result;
}

inline internal Panel_Seek
seek_wrapped_xy(real32 x, real32 y, bool32 round_down){
	Panel_Seek result = {};
	result.type = SEEK_WRAPPED_XY;
	result.x = x;
	result.y = y;
    result.round_down = round_down;
	return result;
}

inline internal Panel_Seek
seek_unwrapped_xy(real32 x, real32 y, bool32 round_down){
	Panel_Seek result = {};
	result.type = SEEK_UNWRAPPED_XY;
	result.x = x;
	result.y = y;
    result.round_down = round_down;
	return result;
}

inline internal Panel_Seek
seek_line_char(i32 line, i32 character){
	Panel_Seek result = {};
	result.type = SEEK_LINE_CHAR;
	result.line = line;
	result.character = character;
	return result;
}

internal View_Cursor_Data
view_cursor_seek(u8 *data, i32 size, Panel_Seek seek,
                 Endline_Mode endline_mode,
                 real32 max_width, Font *font,
                 View_Cursor_Data hint = {}){
    View_Cursor_Data cursor = hint;
    
	while (1){
		View_Cursor_Data prev_cursor = cursor;
        bool32 do_newline = 0;
        bool32 do_slashr = 0;
        u8 c = data[cursor.pos];
        u8 next_c = (cursor.pos+1 < size)?data[cursor.pos+1]:0;
        real32 cw = 0;
		switch (c){
        case '\r':
        {
            switch (endline_mode){
            case ENDLINE_RN_COMBINED:
            {
                if (next_c != '\n'){
                    do_slashr = 1;
                }
            }break;
            case ENDLINE_RN_SEPARATE:
            {
                do_newline = 1;
            }break;
            case ENDLINE_RN_SHOWALLR:
            {
                do_slashr = 1;
            }break;
            }
        }break;
		
        case '\n':
        {
            do_newline = 1;
        }break;
		
        default:
        {
			++cursor.character;
            cw = font->chardata[c].xadvance;
        }break;
		}
        
        if (do_slashr){
            ++cursor.character;
            cw = font->chardata['\\'].xadvance;
            cw += font->chardata['r'].xadvance;
        }
        
        if (cursor.wrapped_x+cw >= max_width){
            cursor.wrapped_y += font->height;
            cursor.wrapped_x = 0;
            prev_cursor = cursor;
        }
        
        cursor.unwrapped_x += cw;
        cursor.wrapped_x += cw;
        
        if (do_newline){
			++cursor.line;
			cursor.unwrapped_y += font->height;
            cursor.wrapped_y += font->height;
            cursor.character = 0;
			cursor.unwrapped_x = 0;
			cursor.wrapped_x = 0;
        }
        
		++cursor.pos;
		
		if (cursor.pos > size){
			cursor = prev_cursor;
			break;
		}
        else{
            bool32 get_out = 0;
            bool32 xy_seek = 0;
            real32 x = 0, y = 0, px = 0;
            switch (seek.type){
            case SEEK_POS:
                if (cursor.pos > seek.pos){
                    cursor = prev_cursor;
                    get_out = 1;
                }break;
                
            case SEEK_WRAPPED_XY:
                x = cursor.wrapped_x; px = prev_cursor.wrapped_x;
                y = cursor.wrapped_y; xy_seek = 1; break;
                
            case SEEK_UNWRAPPED_XY:
                x = cursor.unwrapped_x; px = prev_cursor.unwrapped_x;
                y = cursor.unwrapped_y; xy_seek = 1; break;
                
            case SEEK_LINE_CHAR:
                if (cursor.line == seek.line && cursor.character >= seek.character){
                    get_out = 1;
                }
                else if (cursor.line > seek.line){
                    cursor = prev_cursor;
                    get_out = 1;
                }break;
            }
            if (xy_seek){
                if (seek.round_down){
                    if (y > seek.y || (y > seek.y - font->height && x > seek.x)){
                        cursor = prev_cursor;
                        break;
                    }
                }
                else{
                    if (y > seek.y){
                        cursor = prev_cursor;
                        break;
                    }
                    else if (y > seek.y - font->height && x >= seek.x){
                        real32 cur, prev;
                        cur = x - seek.x;
                        prev = seek.x - px;
                        if (prev < cur) cursor = prev_cursor;
                        break;
                    }
                }
            }
            if (get_out) break;
        }
	}
	
	if (endline_mode == ENDLINE_RN_COMBINED){
		cursor.pos = pos_adjust_to_self(cursor.pos, data, size);
	}
    
	return cursor;
}

internal View_Cursor_Data
view_compute_cursor_from_pos(File_View *view, i32 pos){
    View_Cursor_Data result;
    Editing_File *file = view->file;
    Style *style = view->style;
    Font *font = style->font;
    
    i32 *lines = file->line_starts;
    
    i32 line_index = buffer_get_line_index(file, pos);
    
    i32 line_start = lines[line_index];
    View_Cursor_Data hint;
    hint.pos = line_start;
    hint.line = line_index + 1;
    hint.character = 1;
    hint.unwrapped_y = (real32)line_index*font->height;
    hint.unwrapped_x = 0;
    hint.wrapped_y = view->line_wrap_y[line_index];
    hint.wrapped_x = 0;
    
    real32 max_width = view_compute_width(view);
    result =  view_cursor_seek(file->data, file->size, seek_pos(pos),
                               file->endline_mode, max_width, font, hint);
    
    return result;
}

internal View_Cursor_Data
view_compute_cursor_from_unwrapped_xy(File_View *view, real32 seek_x, real32 seek_y,
                                      bool32 round_down = 0){
    View_Cursor_Data result;
    Editing_File *file = view->file;
    Style *style = view->style;
    Font *font = style->font;
    
    i32 line_index = FLOOR32(seek_y / font->height);
    if (line_index >= file->line_count) line_index = file->line_count - 1;
    
    View_Cursor_Data hint;
    hint.pos = file->line_starts[line_index];
    hint.line = line_index + 1;
    hint.character = 1;
    hint.unwrapped_y = (real32)line_index*font->height;
    hint.unwrapped_x = 0;
    hint.wrapped_y = view->line_wrap_y[line_index];
    hint.wrapped_x = 0;
    
    real32 max_width = view_compute_width(view);
    result = view_cursor_seek(file->data, file->size,
                              seek_unwrapped_xy(seek_x, seek_y, round_down),
                              file->endline_mode, max_width, font, hint);
    
    return result;
}

internal View_Cursor_Data
view_compute_cursor_from_wrapped_xy(File_View *view, real32 seek_x, real32 seek_y,
                                    bool32 round_down = 0){
    View_Cursor_Data result;
    Editing_File *file = view->file;
    Style *style = view->style;
    Font *font = style->font;
    real32 line_height = (real32)font->height;
    
    real32 *line_wrap = view->line_wrap_y;
    i32 line_index;
    // NOTE(allen): binary search lines on wrapped y position
    {
        i32 start = 0;
        i32 end = view->line_count;
        while (1){
            i32 i = (start + end) / 2;
            if (line_wrap[i]+line_height <= seek_y){
                start = i;
            }
            else if (line_wrap[i] > seek_y){
                end = i;
            }
            else{
                line_index = i;
                break;
            }
            if (start >= end - 1){
                line_index = start;
                break;
            }
        }
    }
    
    View_Cursor_Data hint;
    hint.pos = file->line_starts[line_index];
    hint.line = line_index + 1;
    hint.character = 1;
    hint.unwrapped_y = line_index*line_height;
    hint.unwrapped_x = 0;
    hint.wrapped_y = view->line_wrap_y[line_index];
    hint.wrapped_x = 0;
    
    real32 max_width = view_compute_width(view);
    
    result = view_cursor_seek(file->data, file->size,
                              seek_wrapped_xy(seek_x, seek_y, round_down),
                              file->endline_mode, max_width, font, hint);
    
    return result;
}

inline View_Cursor_Data
view_compute_cursor_from_xy(File_View *view, real32 seek_x, real32 seek_y){
    View_Cursor_Data result;
    if (view->unwrapped_lines){
        result = view_compute_cursor_from_unwrapped_xy(view, seek_x, seek_y);
    }
    else{
        result = view_compute_cursor_from_wrapped_xy(view, seek_x, seek_y);
    }
	return result;
}

inline void
view_set_temp_highlight(File_View *view, i32 pos, i32 end_pos){
	view->temp_highlight = view_compute_cursor_from_pos(view, pos);
	view->temp_highlight_end_pos = end_pos;
    view->show_temp_highlight = 1;
}

inline i32
view_get_cursor_pos(File_View *view){
    i32 result;
    if (view->show_temp_highlight){
        result = view->temp_highlight.pos;
    }
    else{
        result = view->cursor.pos;
    }
    return result;
}

inline real32
view_get_cursor_x(File_View *view){
    real32 result;
    View_Cursor_Data *cursor;
    if (view->show_temp_highlight){
        cursor = &view->temp_highlight;
    }
    else{
        cursor = &view->cursor;
    }
    if (view->unwrapped_lines){
        result = cursor->unwrapped_x;
    }
    else{
        result = cursor->wrapped_x;
    }
    return result;
}

inline real32
view_get_cursor_y(File_View *view){
    real32 result;
    View_Cursor_Data *cursor;
    if (view->show_temp_highlight){
        cursor = &view->temp_highlight;
    }
    else{
        cursor = &view->cursor;
    }
    if (view->unwrapped_lines){
        result = cursor->unwrapped_y;
    }
    else{
        result = cursor->wrapped_y;
    }
    return result;
}

internal void
view_set_file(File_View *view, Editing_File *file, Style *style){
    Panel *panel = view->view_base.panel;
    view->file = file;
    
    General_Memory *general = &view->view_base.mem->general;
    Font *font = style->font;
    view->style = style;
    view->font_advance = font->advance;
    view->font_height = font->height;
    
    view_measure_wraps(general, view);
    
    view->cursor = {};
    view->cursor = view_compute_cursor_from_pos(view, file->cursor_pos);
    
    real32 cursor_x, cursor_y;
    real32 w, h;
    real32 target_x, target_y;
    
    cursor_x = view_get_cursor_x(view);
    cursor_y = view_get_cursor_y(view);
    
    w = (real32)(panel->inner.x1 - panel->inner.x0);
    h = (real32)(panel->inner.y1 - panel->inner.y0);
    
    target_x = 0;
    if (cursor_x < target_x){
        target_x = (real32)Max(0, cursor_x - w*.5f);
    }
    else if (cursor_x >= target_x + w){
        target_x = (real32)(cursor_x - w*.5f);
    }
    
    target_y = (real32)FLOOR32(cursor_y - h*.5f);
    if (target_y < 0) target_y = 0;
    
    view->target_x = target_x;
    view->target_y = target_y;
    view->scroll_x = target_x;
    view->scroll_y = target_y;
    
    view->vel_y = 1.f;
    view->vel_x = 1.f;
}

struct Relative_Scrolling{
    real32 scroll_x, scroll_y;
    real32 target_x, target_y;
};

internal Relative_Scrolling
view_get_relative_scrolling(File_View *view){
    Relative_Scrolling result;
    real32 cursor_x, cursor_y;
    cursor_x = view_get_cursor_x(view);
    cursor_y = view_get_cursor_y(view);
    result.scroll_x = cursor_x - view->scroll_x;
    result.scroll_y = cursor_y - view->scroll_y;
    result.target_x = cursor_x - view->target_x;
    result.target_y = cursor_y - view->target_y;
    return result;
}

internal void
view_set_relative_scrolling(File_View *view, Relative_Scrolling scrolling){
    real32 cursor_x, cursor_y;
    cursor_x = view_get_cursor_x(view);
    cursor_y = view_get_cursor_y(view);
    view->scroll_y = cursor_y - scrolling.scroll_y;
    view->target_y = cursor_y - scrolling.target_y;
    if (view->scroll_y < 0) view->scroll_y = 0;
    if (view->target_y < 0) view->target_y = 0;
}

inline void
view_cursor_move(File_View *view, View_Cursor_Data cursor){
	view->cursor = cursor;
    view->preferred_x = view_get_cursor_x(view);
	view->file->cursor_pos = view->cursor.pos;
    view->show_temp_highlight = 0;
}

inline void
view_cursor_move(File_View *view, i32 pos){
	view->cursor = view_compute_cursor_from_pos(view, pos);
    view->preferred_x = view_get_cursor_x(view);
	view->file->cursor_pos = view->cursor.pos;
    view->show_temp_highlight = 0;
}

inline void
view_cursor_move(File_View *view, real32 x, real32 y, bool32 round_down = 0){
    if (view->unwrapped_lines){
        view->cursor = view_compute_cursor_from_unwrapped_xy(view, x, y, round_down);
    }
    else{
        view->cursor = view_compute_cursor_from_wrapped_xy(view, x, y, round_down);
    }
    view->preferred_x = view_get_cursor_x(view);
	view->file->cursor_pos = view->cursor.pos;
    view->show_temp_highlight = 0;
}

inline void
view_set_widget(File_View *view, File_View_Widget_Type type){
    view->widget.type = type;
}

inline i32
view_widget_height(File_View *view, Font *font){
    i32 result = 0;
    switch (view->widget.type){
    case FWIDG_NONE: break;
    case FWIDG_SEARCH: result = font->height + 2; break;
    case FWIDG_GOTO_LINE: result = font->height + 2; break;
    case FWIDG_UNDOING: result = font->height*2; break;
    case FWIDG_RESTORING: result = font->height*2; break;
    }
    return result;
}

inline i32_Rect
view_widget_rect(File_View *view, Font *font){
    Panel *panel = view->view_base.panel;
    i32_Rect whole = panel->inner;
    i32_Rect result;
    result.x0 = whole.x0;
    result.x1 = whole.x1;
    result.y0 = whole.y0 + font->height + 2;
    result.y1 = result.y0 + view_widget_height(view, font);
    
    return result;
}

struct Edit_Spec{
    Edit_Type type;
    i32 start, end;
    u8 *str;
    i32 str_len;
    i32 pre_pos, post_pos;
    i32 next_cursor_pos;
    i32 in_history;
};

internal void
view_pre_replace_range(Mem_Options *mem, File_View *view, Editing_File *file,
                       Editing_Layout *layout, Edit_Spec spec){
    General_Memory *general = &mem->general;
    
    bool32 can_merge = 0, do_merge = 0;
    switch (spec.type){
    case ED_NORMAL:
    {
        if (spec.str_len == 1 && char_is_alpha_numeric(*spec.str)) can_merge = 1;
        if (spec.str_len == 1 && (can_merge || char_is_whitespace(*spec.str))) do_merge = 1;
        
        Edit_Step *step = 0;
        if (spec.in_history != 2){
            step = buffer_post_history(general, file, spec.start, spec.end, spec.str_len,
                                       do_merge, can_merge, ED_REVERSE_NORMAL);
            step->post_pos = spec.next_cursor_pos;
        }
        
        step = buffer_post_undo(general, file, spec.start, spec.end, spec.str_len,
                                do_merge, can_merge, 1);
        step->post_pos = spec.next_cursor_pos;
    }break;
    
    case ED_REVERSE_NORMAL:
    {
        if (spec.in_history != 2)
            buffer_post_history(general, file, spec.start, spec.end, spec.str_len,
                                do_merge, can_merge, ED_NORMAL, spec.pre_pos, spec.post_pos);
        
        buffer_unpost_undo(file);
        
        if (spec.in_history == 1 && file->undo.edit_history_cursor > 0){
            Edit_Step *redo_end = file->undo.history + (file->undo.edit_history_cursor - 1);
            Edit_Step *redo_start = redo_end;
            while (redo_start->reverse_type == ED_REDO) --redo_start;
            ++redo_start;
            ++redo_end;
            
            if (redo_start < redo_end){
                i32 steps_of_redo = (i32)(redo_end - redo_start);
                
                u8 *str_dest_base = file->undo.strings;
                u8 *str_src = file->undo.history_strings + redo_start->str_start;
                i32 str_redo_pos = file->undo.str_redo;
                
                Edit_Step *edit_dest = file->undo.edits + file->undo.edit_redo;
                Edit_Step *edit_src = redo_start;
                
                for (i32 i = 0; i < steps_of_redo; ++i){
                    --edit_dest;
                    edit_dest->range = edit_src->range;
                    edit_dest->pre_pos = edit_src->pre_pos;
                    edit_dest->post_pos = edit_src->post_pos;
                    edit_dest->can_merge = edit_src->can_merge;
                    edit_dest->reverse_type = edit_src->reverse_type;
                    
                    edit_dest->str_len = edit_src->str_len;
                    str_redo_pos -= edit_dest->str_len;
                    edit_dest->str_start = str_redo_pos;
                    
                    memcpy(str_dest_base + str_redo_pos, str_src, edit_dest->str_len);
                    str_src += edit_dest->str_len;
                    ++edit_src;
                }
                
                file->undo.str_redo = str_redo_pos;
                file->undo.edit_redo -= steps_of_redo;
                
                Assert(file->undo.str_redo >= file->undo.str_size);
                Assert(file->undo.edit_redo >= file->undo.edit_count);
            }
        }
    }break;
    
    case ED_UNDO:
    {
        if (spec.in_history != 2)
            buffer_post_history(general, file, spec.start, spec.end, spec.str_len,
                                do_merge, can_merge, ED_REDO, spec.pre_pos, spec.post_pos);
        buffer_post_redo(general, file, spec.start, spec.end, spec.str_len, spec.pre_pos, spec.post_pos);
    }break;
    
    case ED_REDO:
    {
        if (spec.str_len == 1 && char_is_alpha_numeric(*spec.str)) can_merge = 1;
        if (spec.str_len == 1 && (can_merge || char_is_whitespace(*spec.str))) do_merge = 1;
        
        if (spec.in_history != 2)
            buffer_post_history(general, file, spec.start, spec.end, spec.str_len,
                                do_merge, can_merge, ED_UNDO, spec.pre_pos, spec.post_pos);
        
        buffer_post_undo(general, file, spec.start, spec.end, spec.str_len, do_merge, can_merge, 0,
                         spec.pre_pos, spec.post_pos);
    }break;
    }
    
    if (spec.in_history != 2){
        if (spec.type == ED_UNDO){
            if (!file->undo.pending_block){
                file->undo.pending_block = file->undo.edit_history - 1;
                buffer_post_history_block(file, file->undo.pending_block);
            }
        }
        else{
            if (file->undo.pending_block){
                buffer_post_history_block(file, file->undo.edit_history - 1);
                file->undo.pending_block = 0;
            }
        }
    }
    else{
        if (file->undo.pending_block == file->undo.edit_history){
            buffer_unpost_history_block(file);
            file->undo.pending_block = 0;
        }
    }
    
    if (spec.in_history == 0) file->undo.edit_history_cursor = file->undo.edit_history;
}

internal void
view_post_replace_range(Mem_Options *mem, File_View *view, Editing_File *file,
                        Editing_Layout *layout, Edit_Spec spec){
    Panel *current_panel = layout->panels;
    i32 panel_count = layout->panel_count;
    for (i32 i = 0; i < panel_count; ++i, ++current_panel){
        File_View *current_view = view_to_file_view(current_panel->view);
        if (current_view && current_view->file == file){
            view_measure_wraps(&mem->general, current_view);
            if (current_view != view){
                if (current_view->cursor.pos >= spec.end){
                    view_cursor_move(current_view, current_view->cursor.pos+shift.amount);
                }
                else if (current_view->cursor.pos > spec.start){
                    view_cursor_move(current_view, spec.start);
                }
                
                if (current_view->mark >= spec.end){
                    current_view->mark += shift.amount;
                }
                else if (current_view->mark > spec.start){
                    current_view->mark += spec.start;
                }
                current_view->preferred_x = view_get_cursor_x(current_view);
            }
        }
    }
}

internal void
view_replace_range(Mem_Options *mem, File_View *view, Editing_File *file,
                   Editing_Layout *layout, Edit_Spec spec){
    Assert(file);
    
    view_pre_replace_range(mem, view, file, layout, spec);
    
    Shift_Information shift;
    if (!spec.skip_text_replace){
        shift =
            buffer_replace_range(mem, file, spec.start, spec.end, spec.str, spec.str_len);
    }
    else{
        shift = spec.override_replace;
    }
    
    view_post_replace_range(mem, view, file, layout, spec, shift);
}

inline void
view_replace_range(Mem_Options *mem, File_View *view, Editing_Layout *layout,
                   i32 start, i32 end, u8 *str, i32 len, i32 next_cursor_pos){
    Edit_Spec spec = {};
    spec.type = ED_NORMAL;
    spec.start =  start;
    spec.end = end;
    spec.str = str;
    spec.str_len = len;
    spec.next_cursor_pos = next_cursor_pos;
    view_replace_range(mem, view, view->file, layout, spec);
}

inline void
view_range_undo(Mem_Options *mem, File_View *view, Editing_Layout *layout,
                Edit_Step step){
    Editing_File *file = view->file;
    
    Edit_Spec spec = {};
    spec.type = ED_UNDO;
    spec.start =  step.range.start;
    spec.end = step.range.end;
    spec.str = file->undo.strings + step.str_start;
    spec.str_len = step.str_len;
    spec.pre_pos = step.pre_pos;
    spec.post_pos = step.post_pos;
    spec.in_history = 0;
    view_replace_range(mem, view, file, layout, spec);
}

inline void
view_range_redo(Mem_Options *mem, File_View *view, Editing_Layout *layout,
                Edit_Step step){
    Editing_File *file = view->file;
    
    Edit_Spec spec = {};
    spec.type = ED_REDO;
    spec.start =  step.range.start;
    spec.end = step.range.end;
    spec.str = file->undo.strings + step.str_start;
    spec.str_len = step.str_len;
    spec.pre_pos = step.pre_pos;
    spec.post_pos = step.post_pos;
    spec.in_history = 0;
    view_replace_range(mem, view, file, layout, spec);
}

// TODO(allen): should these still be view operations?
internal i32
view_find_end_of_line(File_View *view, i32 pos){
	Editing_File *file = view->file;
	u8 *data = (u8*)file->data;
	while (pos < file->size &&
		   !starts_new_line(data[pos], file->endline_mode)){
		++pos;
	}
	if (pos >= file->size){
		pos = file->size;
	}
	return pos;
}

internal i32
view_find_beginning_of_line(File_View *view, i32 pos){
	Editing_File *file = view->file;
	u8 *data = (u8*)file->data;
	if (pos > 0){
		--pos;
		while (pos > 0 && !starts_new_line(data[pos], file->endline_mode)){
			--pos;
		}
		if (pos != 0){
			++pos;
		}
	}
	return pos;
}

internal i32
view_find_beginning_of_next_line(File_View *view, i32 pos){
	Editing_File *file = view->file;
	u8 *data = (u8*)file->data;
	while (pos < file->size &&
		   !starts_new_line(data[pos], file->endline_mode)){
		++pos;
	}
	if (pos < file->size){
		++pos;
	}
	return pos;
}

internal String*
working_set_next_clipboard_string(General_Memory *general, Working_Set *working, i32 str_size){
	String *result = 0;
	i32 clipboard_current = working->clipboard_current;
	if (working->clipboard_size == 0){
		clipboard_current = 0;
		working->clipboard_size = 1;
	}
	else{
		++clipboard_current;
		if (clipboard_current >= working->clipboard_max_size){
			clipboard_current = 0;
		}
		else if (working->clipboard_size <= clipboard_current){
			working->clipboard_size = clipboard_current+1;
		}
	}
	result = &working->clipboards[clipboard_current];
	working->clipboard_current = clipboard_current;
	working->clipboard_rolling = clipboard_current;
    char *new_str;
	if (result->str){
        new_str = (char*)general_memory_reallocate(general, result->str, result->size, str_size);
    }
    else{
        new_str = (char*)general_memory_allocate(general, str_size+1);
    }
    // TODO(allen): What if new_str == 0?
    *result = make_string(new_str, 0, str_size);
	return result;
}

internal String*
working_set_clipboard_head(Working_Set *working){
	String *result = 0;
	if (working->clipboard_size > 0){
		i32 clipboard_index = working->clipboard_current;
		working->clipboard_rolling = clipboard_index;
		result = &working->clipboards[clipboard_index];
	}
	return result;
}

internal String*
working_set_clipboard_roll_down(Working_Set *working){
	String *result = 0;
	if (working->clipboard_size > 0){
		i32 clipboard_index = working->clipboard_rolling;
		--clipboard_index;
		if (clipboard_index < 0){
			clipboard_index = working->clipboard_size-1;
		}
		working->clipboard_rolling = clipboard_index;
		result = &working->clipboards[clipboard_index];
	}
	return result;
}

inline Editing_File*
working_set_contains(Working_Set *working, String filename){
    Editing_File *result = 0;
    i32 index;
    if (table_find(&working->table, filename, &index)){
        result = working->files + index;
    }
    return result;
}

// TODO(allen): Find a way to choose an ordering for these so it picks better first options.
internal Editing_File*
working_set_lookup_file(Working_Set *working_set, String string){
	Editing_File *file = working_set_contains(working_set, string);
	
	if (!file){
        i32 file_i;
        i32 end = working_set->file_index_count;
        file = working_set->files;
		for (file_i = 0; file_i < end; ++file_i, ++file){
			if (file->live_name.str && (string.size == 0 || has_substr(file->live_name, string))){
				break;
			}
		}
        if (file_i == end) file = 0;
	}
	
	return file;
}

internal void
clipboard_copy(General_Memory *general, Working_Set *working, u8 *data, Range range){
	i32 size = range.end - range.start;
	String *dest = working_set_next_clipboard_string(general, working, size);
	copy(dest, make_string((char*)data + range.start, size));
	system_post_clipboard(*dest);
}

#if 0
internal void
view_endline_convert(Mem_Options *mem, File_View *view,
                     Endline_Convert_Type rn_to,
                     Endline_Convert_Type r_to,
                     Endline_Convert_Type n_to){
    // TODO(allen): Switch over to String
	struct StrSize{
		u8 *str;
		i32 size;
	};
	
	persist StrSize eol_strings[] = {
		{(u8*)"\r\n", 2},
		{(u8*)"\n", 1},
		{(u8*)"\r", 1},
		{(u8*)0, 0}
	};
	
	i32 cursor, mark;
	cursor = view->cursor.pos;
	mark = view->mark;
	
	Editing_File *file = view->file;
	u8 *data = (u8*)file->data;
	for (int i = 0; i < file->size; ++i){
		i32 which = 0;
		if (data[i] == '\r'){
			if (i+1 < file->size && data[i+1] == '\n'){
				which = 1;
			}
			else{
				which = 2;
			}
		}
		else if (data[i] == '\n'){
			which = 3;
		}
		
		// TODO(allen): Replace duplication.
		switch (which){
			case 1:
			{
				if (rn_to != ENDLINE_RN){
					buffer_replace_range(mem, file, i, i+2,
										 eol_strings[rn_to].str,
										 eol_strings[rn_to].size);
					i32 shift = eol_strings[rn_to].size - 2;
					if (cursor >= i){
						cursor += shift;
					}
					if (mark >= i){
						mark += shift;
					}
					i += shift;
				}
				++i;
			}break;
			
			case 2:
			{
				if (r_to != ENDLINE_R){
					buffer_replace_range(mem, file, i, i+1,
										 eol_strings[r_to].str,
										 eol_strings[r_to].size);
					i32 shift = eol_strings[r_to].size - 1;
					if (cursor >= i){
						cursor += shift;
					}
					if (mark >= i){
						mark += shift;
					}
					i += shift;
				}
			}break;
			
			case 3:
			{
				if (n_to != ENDLINE_N){
					buffer_replace_range(mem, file, i, i+1,
										 eol_strings[n_to].str,
										 eol_strings[n_to].size);
					i32 shift = eol_strings[n_to].size - 1;
					if (cursor >= i){
						cursor += shift;
					}
					if (mark >= i){
						mark += shift;
					}
					i += shift;
				}
			}break;
		}
	}
    
	view->cursor = view_compute_cursor_from_pos(view, cursor);
	view->mark = mark;
}
#endif

struct Indent_Definition{
	i32 tabs, spaces;
};

struct Line_Hard_Start{
	bool32 line_is_blank;
	i32 first_character;
};

internal Line_Hard_Start
buffer_find_hard_start(Editing_File *file, i32 line_start){
	Line_Hard_Start result = {};
	result.line_is_blank = 1;
	
	u8 *data = file->data;
	
	for (i32 scan = line_start; scan < file->size; ++scan){
		if (data[scan] == '\n'){
			if (scan > 0 && data[scan-1] == '\r'){
				scan -= 1;
			}
			if (scan < line_start){
				// TODO(allen): This seems wrong.
				scan = line_start;
			}
			result.first_character = scan;
			break;
		}
		else if (!char_is_whitespace(data[scan])){
			result.line_is_blank = 0;
			result.first_character = scan;
			break;
		}
	}
	
	return result;
}

#if 0
// NOTE(allen): Assumes that whitespace_buffer has at least
// indent.tabs*4 + indent.spaces bytes available.
internal Shift_Information
buffer_set_indent_whitespace(Mem_Options *mem, Editing_File *file,
							 Indent_Definition indent,
							 u8 *whitespace_buffer,
                             i32 line_start){
	i32 i = 0;
	while (i < indent.tabs){
        for (i32 j = 0; j < 4; ++j) whitespace_buffer[i++] = ' ';
	}
	while (i < indent.tabs + indent.spaces){
		whitespace_buffer[i++] = ' ';
	}
	
	Range leading_white;
	leading_white.smaller = line_start;
	Line_Hard_Start hard_start;
	hard_start = buffer_find_hard_start(file, line_start);
	leading_white.larger = hard_start.first_character;

    Shift_Information shift = {};
    if (leading_white.smaller < leading_white.larger){
        shift = buffer_replace_range(mem, file,
                                     leading_white.smaller, leading_white.larger,
                                     whitespace_buffer, i);
    }
    
	return shift;
}

inline Indent_Definition
indent_by_width(i32 width, i32 tab_width){
	Indent_Definition indent;
	indent.tabs = 0;
	indent.spaces = width;
	return indent;
}

struct Nest_Level{
	i32 brace_level, paren_level, bracket_level;
};

struct Nest_Level_Hint{
	Nest_Level start_level;
	i32 start_pos;
};

internal Nest_Level
buffer_compute_nest_level_raw(Editing_File *file, i32 pos, Nest_Level_Hint hint = {}){
	Nest_Level result = hint.start_level;
	for (i32 i = hint.start_pos; i < pos; ++i){
		if (file->data[i] == '/' &&
			i+1 < file->size && file->data[i+1] == '*'){
			Seek_Result seek = seek_block_comment_end((char*)file->data, file->size, pos);
			pos = seek.pos;
		}
		else if (file->data[i] == '/' &&
				 i+1 < file->size && file->data[i+1] == '/'){
			Seek_Result seek = seek_unescaped_eol((char*)file->data, file->size, pos);
			pos = seek.pos;
		}
		else if (file->data[i] == '"'){
			Seek_Result seek = seek_unescaped_delim((char*)file->data, file->size, pos, '"');
			pos = seek.pos;
		}
		else if (file->data[i] == '\''){
			Seek_Result seek = seek_unescaped_delim((char*)file->data, file->size, pos, '\'');
			pos = seek.pos;
		}
		switch (file->data[i]){
			case '{':
				++result.brace_level;
				break;
			case '}':
				if (result.brace_level > 0){
					--result.brace_level;
				}
				break;
			case '(':
				++result.paren_level;
				break;
			case ')':
				if (result.paren_level > 0){
					--result.paren_level;
				}
				break;
			case '[':
				++result.bracket_level;
				break;
			case ']':
				if (result.bracket_level > 0){
					--result.bracket_level;
				}
				break;
		}
	}
	return result;
}

internal Nest_Level
buffer_compute_nest_level_tokens(Editing_File *file, i32 pos, Nest_Level_Hint hint = {}){
	Nest_Level result = hint.start_level;
	Cpp_Token_Stack token_stack = file->token_stack;
	i32 start_token;
	{
		Cpp_Get_Token_Result get_result = cpp_get_token(&file->token_stack, hint.start_pos);
		start_token = get_result.token_index;
		if (get_result.in_whitespace){
			++start_token;
		}
	}
	for (i32 i = start_token; i < token_stack.count && token_stack.tokens[i].start < pos; ++i){
		if (token_stack.tokens[i].flags & CPP_TFLAG_PP_BODY){
			continue;
		}
		switch (token_stack.tokens[i].type){
			case CPP_TOKEN_BRACE_OPEN:
				++result.brace_level;
				break;
			case CPP_TOKEN_BRACE_CLOSE:
				if (result.brace_level > 0){
					--result.brace_level;
				}
				break;
			case CPP_TOKEN_PARENTHESE_OPEN:
				++result.paren_level;
				break;
			case CPP_TOKEN_PARENTHESE_CLOSE:
				if (result.paren_level > 0){
					--result.paren_level;
				}
				break;
			case CPP_TOKEN_BRACKET_OPEN:
				++result.bracket_level;
				break;
			case CPP_TOKEN_BRACKET_CLOSE:
				if (result.bracket_level > 0){
					--result.bracket_level;
				}
				break;
		}
	}
	return result;
}
#endif

#if 0
internal void
view_auto_tab(Mem_Options *mem, File_View *view, i32 start, i32 end){
	Editing_File *file = view->file;
	u8 *data = (u8*)file->data;
	
	start = view_find_beginning_of_line(view, start);
	end = view_find_end_of_line(view, end);
	
	i32 cursor = view->cursor.pos;
	i32 mark = view->mark;
	
	Nest_Level_Hint hint = {};
	while (start < end){
		Line_Hard_Start hard_start;
		hard_start = buffer_find_hard_start(file, start);
		
		i32 x_pos = 0;
		
		i32 read_until = hard_start.first_character;
		u8 first_character = file->data[read_until];
		if (first_character == '}' ||
			first_character == ')' ||
			first_character == ']'){
			++read_until;
		}
		
		Nest_Level nesting;
		bool32 is_label = 0;
		bool32 is_continuation = 0;
		bool32 is_preprocessor = 0;
		bool32 is_string_continuation = 0;
		AllowLocal(is_string_continuation);
		// TODO(allen): Better system for finding out what extra data types a file has?
		if (file->tokens_complete){
			nesting = buffer_compute_nest_level_tokens(file, read_until, hint);
			
			i32 colon_expect_level = 1;
			
			Cpp_Get_Token_Result token_get =
				cpp_get_token(&file->token_stack, hard_start.first_character);
			
			i32 token_i;
			
			{
				if (token_get.in_whitespace){
					token_i = -1;
				}
				else{
					token_i = token_get.token_index;
				}
			}
			
			while (colon_expect_level != 0){
				Cpp_Token_Type type = CPP_TOKEN_JUNK;
				if (token_i >= 0 && token_i < file->token_stack.count){
					type = file->token_stack.tokens[token_i].type;
				}
                
                if (is_keyword(type)){
                    Cpp_Token *token = file->token_stack.tokens + token_i;
                    String lexeme = make_string((char*)file->data + token->start, token->size);
                    if (colon_expect_level == 1){
                        if (match("case", lexeme)){
                            colon_expect_level = 2;
                        }
                        else if (match("public", lexeme)){
                            colon_expect_level = 3;
                        }
                        else if (match("private", lexeme)){
                            colon_expect_level = 3;
                        }
                        else if (match("protected", lexeme)){
                            colon_expect_level = 3;
                        }
                    }
                    else{
                        colon_expect_level = 0;
                    }
                }
                else{
                    switch (type){
					case CPP_TOKEN_IDENTIFIER:
					{
						colon_expect_level = 3;
					}break;
                    
					case CPP_TOKEN_COLON:
					{
						if (colon_expect_level == 3){
							is_label = 1;
							colon_expect_level = 0;
						}
					}break;
                    
					default:
					{
						colon_expect_level = 0;
					}break;
                    }
                }
				
				++token_i;
			}
			
			if (!token_get.in_whitespace){
				Cpp_Token *token = file->token_stack.tokens + token_get.token_index;
				if (token->type == CPP_TOKEN_STRING_CONSTANT ||
					token->type == CPP_TOKEN_CHARACTER_CONSTANT){
					is_string_continuation = 1;
				}
			}
			
			if (!is_string_continuation){
				token_i = token_get.token_index;
				bool32 in_whitespace = token_get.in_whitespace;
				if (token_i >= 0){
					Cpp_Token *token = file->token_stack.tokens + token_i;
                    
					bool32 never_continuation = 0;
					if (!in_whitespace){
						switch (token->type){
							case CPP_TOKEN_BRACE_CLOSE:
							case CPP_TOKEN_BRACE_OPEN:
							case CPP_TOKEN_PARENTHESE_OPEN:
							case CPP_TOKEN_PARENTHESE_CLOSE:
							case CPP_TOKEN_COMMENT:
								never_continuation = 1;
								break;
						}
						if (token->flags & CPP_TFLAG_PP_DIRECTIVE ||
							token->flags & CPP_TFLAG_PP_BODY){
							never_continuation = 1;
						}
					}
					else{
						++token_i;
					}
                    
					if (!never_continuation){
						--token_i;
						token = file->token_stack.tokens + token_i;
						bool32 keep_looping = 1;
						is_continuation = 1;
						while (token_i >= 0 && keep_looping){
							--token_i;
							keep_looping = 0;
							if (token->flags & CPP_TFLAG_PP_DIRECTIVE ||
								token->flags & CPP_TFLAG_PP_BODY){
								keep_looping = 1;
							}
							else{
								switch (token->type){
									case CPP_TOKEN_BRACE_CLOSE:
									case CPP_TOKEN_BRACE_OPEN:
									case CPP_TOKEN_PARENTHESE_OPEN:
									case CPP_TOKEN_SEMICOLON:
									case CPP_TOKEN_COLON:
									case CPP_TOKEN_COMMA:
										is_continuation = 0;
										break;
								
									case CPP_TOKEN_COMMENT:
									case CPP_TOKEN_JUNK:
										keep_looping = 1;
										break;
								}
							}
						}
						if (token_i == -1){
							is_continuation = 0;
						}
					}
				}
			}
			
			token_i = token_get.token_index;
			if (token_i >= 0){
				Cpp_Token *token = file->token_stack.tokens + token_i;
				if (!token_get.in_whitespace){
					if (token->flags & CPP_TFLAG_PP_DIRECTIVE ||
						token->flags & CPP_TFLAG_PP_BODY){
						is_preprocessor = 1;
					}
				}
			}
		}
		
		else{
			nesting = buffer_compute_nest_level_raw(file, read_until, hint);
		}
		
		hint.start_level = nesting;
		hint.start_pos = read_until;

		if (is_preprocessor || is_string_continuation){
			x_pos = 0;
		}
		else{
			x_pos = 4*(nesting.brace_level + nesting.bracket_level + nesting.paren_level);
			if (is_continuation){
				x_pos += 4;
			}
			if (is_label){
				x_pos -= 4;
			}
			if (x_pos < 0){
				x_pos = 0;
			}
		}
		
		// TODO(allen): Need big transient memory for this operation.
		// NOTE(allen): This is temporary. IRL it should probably come
		// from transient memory space.
		u8 whitespace[200];
		
		Indent_Definition indent;
        // TODO(allen): Revist all this.
		indent = indent_by_width(x_pos, 4);
		
		TentativeAssert(indent.tabs + indent.spaces < 200);
		
		Shift_Information shift;
		shift = buffer_set_indent_whitespace(mem, file, indent, whitespace, start);
		
		if (hint.start_pos >= shift.start){
			hint.start_pos += shift.amount;
		}
		
		if (cursor >= shift.start && cursor <= shift.end){
			Line_Hard_Start hard_start;
			hard_start = buffer_find_hard_start(file, shift.start);
			cursor = hard_start.first_character;
		}
		else if (cursor > shift.end){
			cursor += shift.amount;
		}
		if (mark >= shift.start && mark <= shift.end){
			Line_Hard_Start hard_start;
			hard_start = buffer_find_hard_start(file, shift.start);
			mark = hard_start.first_character;
		}
		else if (mark > shift.end){
			mark += shift.amount;
		}
		
		start = view_find_beginning_of_next_line(view, start);
		end += shift.amount;
	}
	
	view_cursor_move(view, cursor);
	view->mark = pos_adjust_to_self(mark, data, file->size);
}
#endif

internal u32*
style_get_color(Style *style, Cpp_Token token){
	u32 *result;
    if (token.flags & CPP_TFLAG_IS_KEYWORD){
        if (token.type == CPP_TOKEN_BOOLEAN_CONSTANT){
			result = &style->main.bool_constant_color;
        }
        else{
            result = &style->main.keyword_color;
        }
    }
    else if(token.flags & CPP_TFLAG_PP_DIRECTIVE){
        result = &style->main.preproc_color;
    }
    else{
        switch (token.type){
		case CPP_TOKEN_COMMENT:
			result = &style->main.comment_color;
			break;
            
		case CPP_TOKEN_STRING_CONSTANT:
			result = &style->main.str_constant_color;
            break;
            
		case CPP_TOKEN_CHARACTER_CONSTANT:
			result = &style->main.char_constant_color;
            break;
            
		case CPP_TOKEN_INTEGER_CONSTANT:
			result = &style->main.int_constant_color;
            break;
            
		case CPP_TOKEN_FLOATING_CONSTANT:
			result = &style->main.float_constant_color;
            break;
            
		case CPP_TOKEN_INCLUDE_FILE:
			result = &style->main.include_color;
            break;
            
		default:
			result = &style->main.default_color;
        }
    }
	return result;
}

internal bool32
smooth_camera_step(real32 *target, real32 *current, real32 *vel, real32 S, real32 T){
    bool32 result = 0;
    real32 targ = *target;
    real32 curr = *current;
    real32 v = *vel;
    if (curr != targ){
        if (curr > targ - .1f && curr < targ + .1f){
            curr = targ;
            v = 1.f;
        }
        else{
            real32 L = lerp(curr, T, targ);
            
            i32 sign = (targ > curr) - (targ < curr);
            real32 V = curr + sign*v;
            
            if (sign > 0) curr = Min(L, V);
            else curr = Max(L, V);
            
            if (curr == V){
                v *= S;
            }
        }
        
        *target = targ;
        *current = curr;
        *vel = v;
        result = 1;
    }
    return result;
}

inline real32
view_compute_max_target_y(i32 lowest_line, i32 line_height, real32 view_height){
    real32 max_target_y = ((lowest_line+.5f)*line_height) - view_height*.5f;
    return max_target_y;
}

internal real32
view_compute_max_target_y(File_View *view){
    i32 lowest_line = view_compute_lowest_line(view);
    i32 line_height = view->style->font->height;
    real32 view_height = view_compute_height(view);
    real32 max_target_y = view_compute_max_target_y(
        lowest_line, line_height, view_height);
    return max_target_y;
}

internal void
remeasure_file_view(View *view_, i32_Rect rect){
    File_View *view = (File_View*)view_;
    Relative_Scrolling relative = view_get_relative_scrolling(view);
    view_measure_wraps(&view->view_base.mem->general, view);
    view_cursor_move(view, view->cursor.pos);
    view->preferred_x = view_get_cursor_x(view);
    view_set_relative_scrolling(view, relative);
}

internal bool32
do_undo_slider(Widget_ID wid, UI_State *state, UI_Layout *layout, i32 max, i32 v, Undo_Data *undo, i32 *out){
    bool32 result = 0;
    Font *font = state->font;
    i32 character_h = font->height;
    
    i32_Rect containing_rect = layout_rect(layout, character_h * 2);

    i32_Rect click_rect;
    click_rect.x0 = containing_rect.x0 + character_h - 1;
    click_rect.x1 = containing_rect.x1 - character_h + 1;
    click_rect.y0 = containing_rect.y0 + (character_h/2) - 1;
    click_rect.y1 = containing_rect.y1 - DIVCEIL32(character_h, 2) + 1;
    
    if (state->input_stage){
        real32 l;
        if (ui_do_slider_input(state, click_rect, wid, (real32)click_rect.x0, (real32)click_rect.x1, &l)){
            real32 v_new = lerp(0.f, l, (real32)max);
            v = ROUND32(v_new);
            result = 1;
            if (out) *out = v;
        }
    }
    else{
        if (max > 0){
            Render_Target *target = state->target;
            UI_Style ui_style = get_ui_style_upper(state->style);
            
            real32 L = unlerp(0.f, (real32)v, (real32)max);
            i32 x = FLOOR32(lerp((real32)click_rect.x0, L, (real32)click_rect.x1));
            
            i32 bar_top = containing_rect.y0 + character_h - 1;
            i32 bar_bottom = containing_rect.y1 - character_h + 1;
            
            bool32 show_bar = 1;
            real32 tick_step = (click_rect.x1 - click_rect.x0) / (real32)max;
            bool32 show_ticks = 1;
            if (tick_step <= 5.f) show_ticks = 0;
            
            if (undo == 0){
                if (show_bar){
                    i32_Rect slider_rect;
                    slider_rect.x0 = click_rect.x0;
                    slider_rect.x1 = x;
                    slider_rect.y0 = bar_top;
                    slider_rect.y1 = bar_bottom;
                    
                    draw_rectangle(target, slider_rect, ui_style.dim);
                    
                    slider_rect.x0 = x;
                    slider_rect.x1 = click_rect.x1;
                    draw_rectangle(target, slider_rect, ui_style.pop1);
                }
                
                if (show_ticks){
                    real32_Rect tick;
                    tick.x0 = (real32)click_rect.x0 - 1;
                    tick.x1 = (real32)click_rect.x0 + 1;
                    tick.y0 = (real32)bar_top - 3;
                    tick.y1 = (real32)bar_bottom + 3;
                    
                    for (i32 i = 0; i < v; ++i){
                        draw_rectangle(target, tick, ui_style.dim);
                        tick.x0 += tick_step;
                        tick.x1 += tick_step;
                    }
                    
                    for (i32 i = v; i <= max; ++i){
                        draw_rectangle(target, tick, ui_style.pop1);
                        tick.x0 += tick_step;
                        tick.x1 += tick_step;
                    }
                }
            }
            else{
                
                if (show_bar){
                    i32_Rect slider_rect;
                    slider_rect.x0 = click_rect.x0;
                    slider_rect.y0 = bar_top;
                    slider_rect.y1 = bar_bottom;

                    Edit_Step *history = undo->history;
                    i32 block_count = undo->history_block_count;
                    Edit_Step *step = history;
                    for (i32 i = 0; i < block_count; ++i){
                        u32 color;
                        if (step->reverse_type == ED_REDO) color = ui_style.pop1;
                        else color = ui_style.dim;
                        
                        real32 L;
                        if (i + 1 == block_count){
                            L = 1.f;
                        }else{
                            step = history + step->next_block;
                            L = unlerp(0.f, (real32)(step - history), (real32)max);
                        }
                        i32 x = FLOOR32(lerp((real32)click_rect.x0, L, (real32)click_rect.x1));
                        
                        slider_rect.x1 = x;
                        draw_rectangle(target, slider_rect, color);
                        slider_rect.x0 = slider_rect.x1;
                    }
                }
                
                if (show_ticks){
                    real32_Rect tick;
                    tick.x0 = (real32)click_rect.x0 - 1;
                    tick.x1 = (real32)click_rect.x0 + 1;
                    tick.y0 = (real32)bar_top - 3;
                    tick.y1 = (real32)bar_bottom + 3;

                    Edit_Step *history = undo->history;
                    u32 color = ui_style.dim;
                    for (i32 i = 0; i <= max; ++i){
                        if (i != max){
                            if (history[i].reverse_type == ED_REDO) color = ui_style.pop1;
                            else color = ui_style.dim;
                        }
                        draw_rectangle(target, tick, color);
                        tick.x0 += tick_step;
                        tick.x1 += tick_step;
                    }
                }
            }
            
            i32_Rect slider_handle;
            slider_handle.x0 = x - 2;
            slider_handle.x1 = x + 2;
            slider_handle.y0 = bar_top - (character_h/2);
            slider_handle.y1 = bar_bottom + (character_h/2);
            
            draw_rectangle(target, slider_handle, ui_style.bright);
        }
    }
    
    return result;
}

internal void
view_post_paste_effect(File_View *view, i32 ticks, i32 start, i32 size, u32 color){
    view->paste_effect.start = start;
    view->paste_effect.end = start + size;
    view->paste_effect.color = color;
    view->paste_effect.tick_down = ticks;
    view->paste_effect.tick_max = ticks;
}

internal void
view_undo(Mem_Options *mem, Editing_Layout *layout, File_View *view){
    Editing_File *file = view->file;
    
    if (file && file->undo.edit_count > 0){
        Edit_Step step = file->undo.edits[--file->undo.edit_count];
        
        view_range_undo(mem, view, layout, step);
        view_cursor_move(view, step.pre_pos);
        view->mark = view->cursor.pos;
        
        view_post_paste_effect(view, 10, step.range.start, step.str_len,
                               view->style->main.undo_color);
        file->undo.str_size -= step.str_len;
    }
}

internal void
view_redo(Mem_Options *mem, Editing_Layout *layout, File_View *view){
    Editing_File *file = view->file;
    
    if (file->undo.edit_redo < file->undo.edit_max){
        Edit_Step step = file->undo.edits[file->undo.edit_redo++];
        
        view_range_redo(mem, view, layout, step);
        view_cursor_move(view, step.post_pos);
        view->mark = view->cursor.pos;
        
        view_post_paste_effect(view, 10, step.range.start, step.str_len,
                               view->style->main.undo_color);
        
        file->undo.str_redo += step.str_len;
    }
}

internal void
view_history_step(Mem_Options *mem, Editing_Layout *layout, File_View *view, i32 direction){
    Assert(direction == 1 || direction == 2);
    
    Editing_File *file = view->file;
    bool32 do_history_step = 0;
    Edit_Step step = {};
    if (direction == 1){
        if (file->undo.edit_history_cursor > 0){
            do_history_step = 1;
            step = file->undo.history[--file->undo.edit_history_cursor];
        }
    }
    else{
        if (file->undo.edit_history_cursor < file->undo.edit_history){
            Assert(((file->undo.edit_history - file->undo.edit_history_cursor) & 1) == 0);
            step = file->undo.history[--file->undo.edit_history];
            file->undo.str_history -= step.str_len;
            ++file->undo.edit_history_cursor;
            do_history_step = 1;
        }
    }
    
    if (do_history_step){
        Edit_Spec spec = {};
        spec.type = step.reverse_type;
        spec.start = step.range.start;
        spec.end = step.range.end;
        spec.str = file->undo.history_strings + step.str_start;
        spec.str_len = step.str_len;
        spec.pre_pos = step.pre_pos;
        spec.post_pos = step.post_pos;
        spec.next_cursor_pos = step.post_pos;
        spec.in_history = direction;
        
        switch (spec.type){
        case ED_UNDO:
            Assert(file->undo.edit_count > 0);
            --file->undo.edit_count;
            break;
            
        case ED_REDO:
            Assert(file->undo.edit_redo < file->undo.edit_max);
            ++file->undo.edit_redo;
            break;
        }
        
        view_replace_range(mem, view, file, layout, spec);
        
        switch (spec.type){
        case ED_NORMAL:
        case ED_REDO:
            view_cursor_move(view, step.post_pos);
            break;
            
        case ED_REVERSE_NORMAL:
        case ED_UNDO:
            view_cursor_move(view, step.pre_pos);
            break;
        }
        view->mark = view->cursor.pos;
    }
}

internal i32
step_file_view(Thread_Context *thread, View *view_, i32_Rect rect,
               bool32 is_active, Input_Summary *user_input){
    view_->mouse_cursor_type = APP_MOUSE_CURSOR_IBEAM;
    i32 result = 0;
    File_View *view = (File_View*)view_;
    Editing_File *file = view->file;
    Style *style = view->style;
    Font *font = style->font;
    
    real32 line_height = (real32)font->height;
    real32 cursor_y = view_get_cursor_y(view);
    real32 target_y = view->target_y;
    real32 max_y = view_compute_height(view) - line_height*2;
    i32 lowest_line = view_compute_lowest_line(view);
    real32 max_target_y = view_compute_max_target_y(lowest_line, font->height, max_y);
    real32 delta_y = 3.f*line_height;
    real32 extra_top = 0.f;
    extra_top += view_widget_height(view, font);
    real32 taken_top_space = line_height + extra_top;
    
    if (user_input->mouse.my < rect.y0 + taken_top_space){
        view_->mouse_cursor_type = APP_MOUSE_CURSOR_ARROW;
    }
    else{
        view_->mouse_cursor_type = APP_MOUSE_CURSOR_IBEAM;
    }
    
    if (user_input->mouse.wheel_used){
        real32 wheel_multiplier = 3.f;
        real32 delta_target_y = delta_y*user_input->mouse.wheel_amount*wheel_multiplier;
        target_y += delta_target_y;
        
        if (target_y < -taken_top_space) target_y = -taken_top_space;
        if (target_y > max_target_y) target_y = max_target_y;
        
        real32 old_cursor_y = cursor_y;
        if (cursor_y >= target_y + max_y) cursor_y = target_y + max_y;
        if (cursor_y < target_y + taken_top_space) cursor_y = target_y + taken_top_space;
        
        if (cursor_y != old_cursor_y){
            view->cursor =
                view_compute_cursor_from_xy(view,
                                            view->preferred_x,
                                            cursor_y);
        }
        
        result = 1;
    }
    
    while (cursor_y > target_y + max_y) target_y += delta_y;
    while (cursor_y < target_y + taken_top_space) target_y -= delta_y;
    
    if (target_y > max_target_y) target_y = max_target_y;
    if (target_y < -extra_top) target_y = -extra_top;
    view->target_y = target_y;
    
    real32 cursor_x = view_get_cursor_x(view);
    real32 target_x = view->target_x;
    real32 max_x = view_compute_width(view);
    if (cursor_x < target_x){
        target_x = (real32)Max(0, cursor_x - max_x/2);
    }
    else if (cursor_x >= target_x + max_x){
        target_x = (real32)(cursor_x - max_x/2);
    }
    
    view->target_x = target_x;
    
    if (smooth_camera_step(&view->target_y, &view->scroll_y, &view->vel_y, 40.f, 1.f/4.f)){
        result = 1;
    }
    if (smooth_camera_step(&view->target_x, &view->scroll_x, &view->vel_x, 40.f, 1.f/4.f)){
        result = 1;
    }
    if (view->paste_effect.tick_down > 0){
        --view->paste_effect.tick_down;
        result = 1;
    }
    
    if (is_active && user_input->mouse.press_l){
        real32 max_y = view_compute_height(view);
        real32 rx = (real32)(user_input->mouse.mx - rect.x0);
        real32 ry = (real32)(user_input->mouse.my - rect.y0 - line_height - 2);
        
        if (ry >= extra_top){
            view_set_widget(view, FWIDG_NONE);
            if (rx >= 0 && rx < max_x && ry >= 0 && ry < max_y){
                view_cursor_move(view, rx + view->scroll_x, ry + view->scroll_y, 1);
                view->mode = {};
            }
        }
        result = 1;
    }
    
    if (!is_active) view_set_widget(view, FWIDG_NONE);

    // NOTE(allen): framely undo stuff
    {
        i32 scrub_max = view->scrub_max;
        i32 undo_count, redo_count, total_count;
        undo_count = file->undo.edit_count;
        redo_count = file->undo.edit_max - file->undo.edit_redo;
        total_count = undo_count + redo_count;

        switch (view->widget.type){
        case FWIDG_UNDOING:
        case FWIDG_RESTORING:
        {
            i32_Rect widg_rect = view_widget_rect(view, font);
            
            UI_State state = 
                ui_state_init(&view->widget.state, 0, user_input,
                              view->style, 0, 1);
            
            UI_Layout layout;
            begin_layout(&layout, widg_rect);
            
            Widget_ID wid = make_id(&state, 1);

            if (view->widget.type == FWIDG_UNDOING){
                i32 new_count;
                if (do_undo_slider(wid, &state, &layout, total_count, undo_count, 0, &new_count)){
                    view->rewind_speed = 0;
                    view->rewind_amount = 0;
                    
                    for (i32 i = 0; i < scrub_max && new_count < undo_count; ++i){
                        view_undo(view_->mem, view->layout, view);
                        --undo_count;
                    }
                    for (i32 i = 0; i < scrub_max && new_count > undo_count; ++i){
                        view_redo(view_->mem, view->layout, view);
                        ++undo_count;
                    }
                }
            }
            else{
                i32 new_count;
                i32 mid = ((file->undo.edit_history + file->undo.edit_history_cursor) >> 1);
                i32 count = file->undo.edit_history_cursor;
                if (do_undo_slider(wid, &state, &layout, mid, count, &file->undo, &new_count)){
                    for (i32 i = 0; i < scrub_max && new_count < count; ++i){
                        view_history_step(view_->mem, view->layout, view, 1);
                    }
                    for (i32 i = 0; i < scrub_max && new_count > count; ++i){
                        view_history_step(view_->mem, view->layout, view, 2);
                    }
                }
            }
            
            if (ui_finish_frame(&view->widget.state, &state, &layout, widg_rect, 0, 0)){
                result = 1;
            }
        }break;
        }
        
        i32 rewind_max = view->rewind_max;
        view->rewind_amount += view->rewind_speed;
        for (i32 i = 0; view->rewind_amount <= -1.f; ++i, view->rewind_amount += 1.f){
            if (i < rewind_max) view_undo(view_->mem, view->layout, view);
        }
        
        for (i32 i = 0; view->rewind_amount >= 1.f; ++i, view->rewind_amount -= 1.f){
            if (i < rewind_max) view_redo(view_->mem, view->layout, view);
        }
        
        if (view->rewind_speed < 0.f && undo_count == 0){
            view->rewind_speed = 0.f;
            view->rewind_amount = 0.f;
        }
        
        if (view->rewind_speed > 0.f && redo_count == 0){
            view->rewind_speed = 0.f;
            view->rewind_amount = 0.f;
        }
    }
    
    return result;
}

enum File_Sync_State{
    SYNC_GOOD,
    SYNC_BEHIND_OS,
    SYNC_UNSAVED
};

inline File_Sync_State
buffer_get_sync(Editing_File *file){
    File_Sync_State result = SYNC_GOOD;
    if (file->last_4ed_write_time != file->last_sys_write_time)
        result = SYNC_BEHIND_OS;
    else if (file->last_4ed_edit_time > file->last_sys_write_time)
        result = SYNC_UNSAVED;
    return result;
}

internal i32
draw_file_view(Thread_Context *thread, View *view_, i32_Rect rect, bool32 is_active,
               Render_Target *target){
    File_View *view = (File_View*)view_;
    Editing_File *file = view->file;
    Style *style = view->style;
    Font *font = style->font;
    
    Interactive_Bar bar;
    bar.style = style->main.file_info_style;
    bar.font = style->font;
    bar.pos_x = (real32)rect.x0;
    bar.pos_y = (real32)rect.y0;
    bar.text_shift_y = 2;
    bar.text_shift_x = 0;
    bar.rect = rect;
    bar.rect.y1 = bar.rect.y0 + font->height + 2;
    rect.y0 += font->height + 2;
    
    i32 max_x = rect.x1 - rect.x0;
    i32 max_y = rect.y1 - rect.y0 + font->height;
    
    Assert(file && file->data && !file->is_dummy);
    
    i32 size = (i32)file->size;
    u8 *data = (u8*)file->data;
    
    View_Cursor_Data start_cursor;
    start_cursor = view_compute_cursor_from_xy(view, 0, view->scroll_y);
    view->scroll_y_cursor = start_cursor;
    
    i32 start_character = start_cursor.pos;
    
    real32 shift_x = rect.x0 - view->scroll_x;
    real32 shift_y = rect.y0 - view->scroll_y;
    if (view->unwrapped_lines) shift_y += start_cursor.unwrapped_y;
    else shift_y += start_cursor.wrapped_y;
    
    real32 pos_x = 0;
    real32 pos_y = 0;
    
    Cpp_Token_Stack token_stack = file->token_stack;
    u32 highlight_color = 0;
    u32 main_color = style->main.default_color;
    i32 token_i = 0;
    bool32 tokens_use = file->tokens_complete && (file->token_stack.count > 0);
    
    if (tokens_use){
        Cpp_Get_Token_Result result = cpp_get_token(&token_stack, start_character);
        token_i = result.token_index;
        
        main_color = *style_get_color(style, token_stack.tokens[token_i]);
        ++token_i;
    }
    
    for (i32 i = start_character; i <= size; ++i){
        u8 to_render, next_to_render;
        if (i < size){
            to_render = data[i];
        }
        else{
            to_render = 0;
        }
        if (i+1 < size){
            next_to_render = data[i+1];
        }
        else{
            next_to_render = 0;
        }
            
        if (!view->unwrapped_lines && pos_x + font_get_glyph_width(font, to_render) > max_x){
            pos_x = 0;
            pos_y += font->height;
        }
            
        u32 fade_color = 0xFFFF00FF;
        real32 fade_amount = 0.f;
        if (view->paste_effect.tick_down > 0 &&
            view->paste_effect.start <= i && i < view->paste_effect.end){
            fade_color = view->paste_effect.color;
            fade_amount = (real32)(view->paste_effect.tick_down) / view->paste_effect.tick_max;
        }
        
        highlight_color = 0;
        if (tokens_use){
            Cpp_Token current_token = token_stack.tokens[token_i-1];
                
            if (token_i < token_stack.count){
                if (i >= token_stack.tokens[token_i].start){
                    main_color =
                        *style_get_color(style, token_stack.tokens[token_i]);
                    current_token = token_stack.tokens[token_i];
                    ++token_i;
                }
                else if (i >= current_token.start + current_token.size){
                    main_color = 0xFFFFFFFF;
                }
            }
                
            if (current_token.type == CPP_TOKEN_JUNK &&
                i >= current_token.start && i <= current_token.start + current_token.size){
                highlight_color = style->main.highlight_junk_color;
            }
        }
        if (highlight_color == 0 && view->show_whitespace && char_is_whitespace(data[i])){
            highlight_color = style->main.highlight_white_color;
        }
            
        i32 cursor_mode = 0;
        if (view->show_temp_highlight){
            if (view->temp_highlight.pos <= i && i < view->temp_highlight_end_pos){
                cursor_mode = 2;
            }
        }
        else{
            if (view->cursor.pos == i){
                cursor_mode = 1;
            }
        }
            
        real32 to_render_width = font_get_glyph_width(font, to_render);
        real32_Rect cursor_rect =
            real32XYWH(shift_x + pos_x, shift_y + pos_y,
                       1 + to_render_width, (real32)font->height);
            
        if (to_render == '\t' || to_render == 0){
            cursor_rect.x1 = cursor_rect.x0 + font->chardata[' '].xadvance;
        }
            
        if (highlight_color != 0){
            if (file->endline_mode == ENDLINE_RN_COMBINED &&
                to_render == '\r' && next_to_render == '\n'){
                // DO NOTHING
                // NOTE(allen): relevant durring whitespace highlighting
            }
            else{
                real32_Rect highlight_rect = cursor_rect;
                highlight_rect.x0 += 1;
                draw_rectangle(target, highlight_rect, highlight_color);
            }
        }
            
        u32 cursor_color = 0;
        switch (cursor_mode){
        case 1:
            cursor_color = style->main.cursor_color; break;
        case 2:
            cursor_color = style->main.highlight_color; break;
        }
            
        if (is_active){
            if (cursor_color & 0xFF000000) draw_rectangle(target, cursor_rect, cursor_color);
        }
        else{
            if (cursor_color & 0xFF000000)draw_rectangle_outline(target, cursor_rect, cursor_color);
        }
        if (i == view->mark){
            draw_rectangle_outline(target, cursor_rect, style->main.mark_color);
        }
            
        u32 char_color = main_color;
        u32 special_char_color = main_color;
        if (to_render == '\r'){
            special_char_color = char_color = style->main.special_character_color;
        }
        if (is_active){
            switch (cursor_mode){
            case 1:
                char_color = style->main.at_cursor_color; break;
            case 2:
                special_char_color = char_color = style->main.at_highlight_color; break;
            }
        }
            
        char_color = color_blend(char_color, fade_amount, fade_color);
        special_char_color = color_blend(special_char_color, fade_amount, fade_color);
            
        if (to_render == '\r'){
            bool32 show_slashr = 0;
            switch (file->endline_mode){
            case ENDLINE_RN_COMBINED:
            {
                if (next_to_render != '\n'){
                    show_slashr = 1;
                }
            }break;
                
            case ENDLINE_RN_SEPARATE:
            {
                pos_x = 0;
                pos_y += font->height;
            }break;
                
            case ENDLINE_RN_SHOWALLR:
            {
                show_slashr = 1;
            }break;
            }
                
            if (show_slashr){
                font_draw_glyph(target, font, '\\',
                                shift_x + pos_x,
                                shift_y + pos_y,
                                char_color);
                pos_x += font_get_glyph_width(font, '\\');
                    
                real32 cw = font_get_glyph_width(font, 'r');
                draw_rectangle(target,
                               real32XYWH(shift_x + pos_x,
                                          shift_y + pos_y,
                                          cw, (real32)font->height),
                               highlight_color);
                font_draw_glyph(target, font, 'r',
                                shift_x + pos_x,
                                shift_y + pos_y,
                                special_char_color);
                pos_x += cw;
            }
        }
            
        else if (to_render == '\n'){
            pos_x = 0;
            pos_y += font->height;
        }
            
        else if (font->glyphs[to_render].exists){
            font_draw_glyph(target, font, to_render,
                            shift_x + pos_x,
                            shift_y + pos_y,
                            char_color);
            pos_x += to_render_width;
        }
            
        else{
            pos_x += to_render_width;
        }
            
        if (pos_y > max_y){
            break;
        }
    }
    
    if (view->widget.type != FWIDG_NONE){
        UI_Style ui_style = get_ui_style_upper(style);
        
        Font *font = style->font;
        i32_Rect widg_rect = view_widget_rect(view, font);
        
        draw_rectangle(target, widg_rect, ui_style.dark);
        draw_rectangle_outline(target, widg_rect, ui_style.dim);
        
        UI_State state =
            ui_state_init(&view->widget.state, target, 0,
                          view->style, 0, 0);
        
        UI_Layout layout;
        begin_layout(&layout, widg_rect);
        
        switch (view->widget.type){
        case FWIDG_UNDOING:
        case FWIDG_RESTORING:
        {
            Widget_ID wid = make_id(&state, 1);
            if (view->widget.type == FWIDG_UNDOING){
                i32 undo_count, redo_count, total_count;
                undo_count = file->undo.edit_count;
                redo_count = file->undo.edit_max - file->undo.edit_redo;
                total_count = undo_count + redo_count;
                do_undo_slider(wid, &state, &layout, total_count, undo_count, 0, 0);
            }
            else{
                i32 new_count;
                i32 mid = ((file->undo.edit_history + file->undo.edit_history_cursor) >> 1);
                do_undo_slider(wid, &state, &layout, mid, file->undo.edit_history_cursor, &file->undo, &new_count);
            }
        }break;
        
        case FWIDG_SEARCH:
        {
            Widget_ID wid = make_id(&state, 1);
            persist String search_str = make_lit_string("I-Search: ");
            persist String rsearch_str = make_lit_string("Reverse-I-Search: ");
            if (view->isearch.reverse){
                do_text_field(wid, &state, &layout, rsearch_str, view->isearch.str);
            }
            else{
                do_text_field(wid, &state, &layout, search_str, view->isearch.str);
            }
        }break;
        
        case FWIDG_GOTO_LINE:
        {
            Widget_ID wid = make_id(&state, 1);
            persist String gotoline_str = make_lit_string("Goto Line: ");
            do_text_field(wid, &state, &layout, gotoline_str, view->isearch.str);
        }break;
        }
        
        ui_finish_frame(&view->widget.state, &state, &layout, widg_rect, 0, 0);
    }
    
    {
        u32 back_color = bar.style.bar_color;
        draw_rectangle(target, bar.rect, back_color);
        
        u32 base_color = bar.style.base_color;
        intbar_draw_string(target, &bar, file->live_name, base_color);
        intbar_draw_string(target, &bar, make_lit_string(" - "), base_color);
        
        char line_number_space[30];
        String line_number = make_string(line_number_space, 0, 30);
        append(&line_number, "L#");
        append_int_to_str(view->cursor.line, &line_number);
        
        intbar_draw_string(target, &bar, line_number, base_color);

        switch (buffer_get_sync(file)){
        case SYNC_BEHIND_OS:
        {
            persist String out_of_sync = make_lit_string(" BEHIND OS");
            intbar_draw_string(target, &bar, out_of_sync, bar.style.pop2_color);
        }break;
        
        case SYNC_UNSAVED:
        {
            persist String out_of_sync = make_lit_string(" *");
            intbar_draw_string(target, &bar, out_of_sync, bar.style.pop2_color);
        }break;
        }
    }
    
    return 0;
}

internal void
kill_buffer(General_Memory *general, Editing_File *file, Live_Views *live_set, Editing_Layout *layout){
    i32 panel_count = layout->panel_count;
    Panel *panels = layout->panels, *panel;
    panel = panels;
    
    for (i32 i = 0; i < panel_count; ++i){
        View *view = panel->view;
        if (view){
            View *to_kill = view;
            if (view->is_minor) to_kill = view->major;
            File_View *fview = view_to_file_view(to_kill);
            if (fview && fview->file == file){
                live_set_free_view(live_set, &fview->view_base);
                if (to_kill == view) panel->view = 0;
                else view->major = 0;
            }
        }
        ++panel;
    }
    
    buffer_close(general, file);
    buffer_get_dummy(file);
}

internal void
command_search(Command_Data*,Command_Binding);
internal void
command_rsearch(Command_Data*,Command_Binding);

internal
HANDLE_COMMAND_SIG(handle_command_file_view){
    File_View *file_view = (File_View*)(view);
    Editing_File *file = file_view->file;
            
    switch (file_view->widget.type){
    case FWIDG_NONE:
    case FWIDG_UNDOING:
    case FWIDG_RESTORING:
    {
        file_view->next_mode = {};
        if (binding.function) binding.function(command, binding);
        file_view->mode = file_view->next_mode;
        
        if (key.key.keycode == codes->esc)
            view_set_widget(file_view, FWIDG_NONE);
    }break;
    
    case FWIDG_SEARCH:
    {
        String *string = &file_view->isearch.str;
        Single_Line_Input_Step result =
            app_single_line_input_step(codes, key, string);
        
        if (result.made_a_change ||
            binding.function == command_search ||
            binding.function == command_rsearch){
            bool32 step_forward = 0;
            bool32 step_backward = 0;
            
            if (binding.function == command_search){
                step_forward = 1;
            }
            if (binding.function == command_rsearch){
                step_backward = 1;
            }
            
            i32 start_pos = file_view->isearch.pos;
            if (step_forward){
                if (file_view->isearch.reverse){
                    start_pos = file_view->temp_highlight.pos - 1;
                    file_view->isearch.pos = start_pos;
                    file_view->isearch.reverse = 0;
                }
            }
            if (step_backward){
                if (!file_view->isearch.reverse){
                    start_pos = file_view->temp_highlight.pos + 1;
                    file_view->isearch.pos = start_pos;
                    file_view->isearch.reverse = 1;
                }
            }
            
            String file_string = make_string((char*)file->data, file->size);
            i32 pos;
            if (file_view->isearch.reverse){
                if (result.hit_backspace){
                    start_pos = file_view->temp_highlight.pos + 1;
                    file_view->isearch.pos = start_pos;
                }
                else{
                    pos = rfind_substr(file_string, start_pos - 1, *string);
                    if (pos >= 0){
                        if (step_backward){ // TODO(allen): this if and it's mirror are suspicious
                            file_view->isearch.pos = pos;
                            start_pos = pos;
                            pos = rfind_substr(file_string, start_pos - 1, *string);
                            if (pos == -1){
                                pos = start_pos;
                            }
                        }
                        view_set_temp_highlight(file_view, pos, pos+string->size);
                    }
                }
            }
            
            else{
                if (result.hit_backspace){
                    start_pos = file_view->temp_highlight.pos - 1;
                    file_view->isearch.pos = start_pos;
                }
                else{
                    pos = find_substr(file_string, start_pos + 1, *string);
                    if (pos < file->size){
                        if (step_forward){
                            file_view->isearch.pos = pos;
                            start_pos = pos;
                            pos = find_substr(file_string, start_pos + 1, *string);
                            if (pos == file->size){
                                pos = start_pos;
                            }
                        }
                        view_set_temp_highlight(file_view, pos, pos+string->size);
                    }
                }
            }
        }
        
        if (result.hit_newline || result.hit_ctrl_newline){
            view_cursor_move(file_view, file_view->temp_highlight);
            view_set_widget(file_view, FWIDG_NONE);
        }
        
        if (result.hit_esc){
            file_view->show_temp_highlight = 0;
            view_set_widget(file_view, FWIDG_NONE);
        }
    }break;
    
    case FWIDG_GOTO_LINE:
    {
        String *string = &file_view->gotoline.str;
        Single_Line_Input_Step result =
            app_single_number_input_step(codes, key, string);
        
        if (result.hit_newline || result.hit_ctrl_newline){
            i32 line_number = str_to_int(*string);
            Font *font = file_view->style->font;
            if (line_number < 1) line_number = 1;
            file_view->cursor =
                view_compute_cursor_from_unwrapped_xy(file_view, 0, (real32)(line_number-1)*font->height);
            file_view->preferred_x = view_get_cursor_x(file_view);
            
            view_set_widget(file_view, FWIDG_NONE);
        }
        
        if (result.hit_esc){
            view_set_widget(file_view, FWIDG_NONE);
        }
    }break;
    }
}

inline void
free_file_view(View *view_){
    File_View *view = (File_View*)view_;
    general_memory_free(&view_->mem->general, view->line_wrap_y);
}

internal
DO_VIEW_SIG(do_file_view){
    i32 result = 0;
    switch (message){
    case VMSG_RESIZE:
    case VMSG_STYLE_CHANGE:
    {
        remeasure_file_view(view, rect);
    }break;
    case VMSG_DRAW:
    {
        result = draw_file_view(thread, view, rect, (view == active), target);
    }break;
    case VMSG_STEP:
    {
        result = step_file_view(thread, view, rect, (view == active), user_input);
    }break;
    case VMSG_FREE:
    {
        free_file_view(view);
    }break;
    }
    return result;
}

internal File_View*
file_view_init(View *view, Delay *delay, Editing_Layout *layout){
    view->type = VIEW_TYPE_FILE;
    view->do_view = do_file_view;
    view->handle_command = handle_command_file_view;
    
    File_View *result = (File_View*)view;
    result->delay = delay;
    result->layout = layout;
    result->rewind_max = 4;
    result->scrub_max = 1;
    return result;
}

// BOTTOM

