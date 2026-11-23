#include "screens.h"
#include "remote_history.h"
#include "main.h"
#include "define.h"
#include "utility.h"
#include <stdio.h>
#include <string.h>

/* historyScreenMode values. */
#define MODE_PICKER 0
#define MODE_LIST   1
#define MODE_DETAIL 2

#define ROWS_PER_PAGE 4
#define DETAIL_FIELDS 6

extern SystemInfo_t systemInfo;
extern DeviceInfo_t deviceBuff[MAXIMUM_DEVICE];

uint8_t historyScreenMode;

static uint8_t chosenDevice;
/* Frozen snapshot of the chosen device's records, newest first. */
static uint16_t selected;
static uint16_t count;
static uint16_t slots[HJ_SLOTS];
static uint32_t snapshotGeneration[HJ_PAGES];
static uint32_t snapshotRevision;
static UG_TEXTBOX *boxes[] = {&textbox_1, &textbox_2, &textbox_3, &textbox_4, &textbox_5,
    &textbox_6, &textbox_7, &textbox_8, &textbox_9, &textbox_10, &textbox_11, &textbox_12,
    &textbox_13, &textbox_14, &textbox_15, &textbox_16};

static void init(void);
static void render(void);
static void leftClicked(void);
static void rightClicked(void);
static void centerClicked(void);
static void rightPressed(void);
static void centerPressed(void);
static void noAction(void);
static void renderPicker(char *text, uint16_t size);
static void renderList(char *text, uint16_t size);
static void renderDetail(char *text, uint16_t size);
static void windowCallback(UG_MESSAGE *message);
static const HistoryRecord *raw(uint16_t slot);
static uint32_t pageGeneration(uint16_t p);
static uint8_t available(uint8_t id);
static const HistoryRecord *view(uint16_t index);
static void snapshot(void);
static char *powerName(uint8_t state);
static char *actionName(uint8_t action);
static char *sourceName(uint8_t source);
static char *errorName(uint16_t error);
static char *resultName(uint8_t result);
static void createText(uint8_t id, int x, int y, int xe, int ye, char *text, UG_COLOR bg);
static void show(uint8_t id, uint8_t visible);
static void rebuild(void);
static void move(int8_t direction);

/* LEFT/RIGHT move, CENTER opens; hold RIGHT to go back, hold CENTER to refresh. */
Screen_t historyScreen = {
    0,
    init,
    render,
    leftClicked,
    rightClicked,
    centerClicked,
    noAction,
    noAction,
    noAction,
    rightPressed,
    centerPressed,
    noAction,
    noAction
};

/* Public API =============================================================== */

void Screen_OpenHistory(void)
{
    /* Start on the current device, or the first one with history. */
    chosenDevice = systemInfo.currentDeviceId < MAXIMUM_DEVICE ? systemInfo.currentDeviceId : 0;
    if (!available(chosenDevice))
    {
        for (uint8_t id = 0; id < MAXIMUM_DEVICE; id++)
        {
            if (available(id))
            {
                chosenDevice = id;
                break;
            }
        }
    }
    historyScreenMode = MODE_PICKER;

    UG_WindowHide(&window_1);
    UG_WindowDelete(&window_1);
    currentScreen = &historyScreen;
    init();
    render();
}

/* Screen handlers ========================================================== */

static void init(void)
{
    int y;

    /* Window and status bar. */
    UG_WindowCreate(&window_1, obj_buff_wnd_1, MAX_OBJECTS, windowCallback);
    UG_WindowSetStyle(&window_1, WND_STYLE_HIDE_TITLE);
    UG_WindowSetTitleHeight(&window_1, 0);
    UG_WindowSetBackColor(&window_1, C_BLACK);
    UG_WindowSetXStart(&window_1, 0);
    UG_WindowSetYStart(&window_1, 0);
    UG_WindowSetXEnd(&window_1, 127);
    UG_WindowSetYEnd(&window_1, 159);
    Screen_StatusBarCreate();

    /* Button labels depend on the mode. */
    createText(2, 0, 144, 35, 159, historyScreenMode == MODE_DETAIL ? "Prev" : "Up", C_DARK_SLATE_BLUE);
    createText(3, 36, 144, 91, 159, historyScreenMode == MODE_PICKER ? "Open" :
        historyScreenMode == MODE_LIST ? "Detail" : "List", C_DARK_CYAN);
    createText(4, 92, 144, 127, 159, historyScreenMode == MODE_DETAIL ? "Next" : "Down", C_DARK_SLATE_BLUE);
    for (uint8_t id = 2; id <= 4; id++)
    {
        UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER);
    }

    /* Title, page indicator and eight content lines. */
    createText(5, 0, 20, 77, 37, "", C_DARK_SLATE_BLUE);
    createText(6, 78, 20, 127, 37, "", C_DARK_SLATE_BLUE);
    UG_TextboxSetAlignment(&window_1, 6, ALIGN_CENTER_RIGHT);
    for (uint8_t i = 0; i < 8; i++)
    {
        y = historyScreenMode == MODE_DETAIL ? 43 + i * 13 : 43 + (i / 2) * 22 + (i % 2) * 10;
        createText(7 + i, 4, y, 123, y + 9, "", C_BLACK);
    }
    createText(15, 0, 132, 127, 141, "Hold RIGHT: Back", C_BLACK);
    UG_TextboxSetAlignment(&window_1, 15, ALIGN_CENTER);
    UG_WindowShow(&window_1);
}

static void render(void)
{
    char text[64];

    Screen_StatusBarUpdate();
    snprintf(text, sizeof(text), historyScreenMode == MODE_PICKER ? "Select device" : "M%u1 %s",
        chosenDevice + 1, historyScreenMode == MODE_LIST ? "History" : "Event");
    UG_TextboxSetText(&window_1, 5, text);

    if (historyScreenMode == MODE_PICKER)
    {
        renderPicker(text, sizeof(text));
    }
    else if (historyScreenMode == MODE_LIST)
    {
        renderList(text, sizeof(text));
    }
    else
    {
        renderDetail(text, sizeof(text));
    }

    UG_TextboxSetText(&window_1, 15, historyScreenMode && snapshotRevision != remoteHistoryRevision ?
        "New: Hold CTR refresh" : "Hold RIGHT: Back");
    UG_Update();
}

static void leftClicked(void)
{
    move(-1);
}

static void rightClicked(void)
{
    move(1);
}

/* Picker -> list -> detail -> list. */
static void centerClicked(void)
{
    if (historyScreenMode == MODE_PICKER)
    {
        if (!available(chosenDevice))
        {
            return;
        }
        snapshot();
        historyScreenMode = MODE_LIST;
    }
    else if (historyScreenMode == MODE_LIST)
    {
        if (!count)
        {
            return;
        }
        historyScreenMode = MODE_DETAIL;
    }
    else
    {
        historyScreenMode = MODE_LIST;
    }
    rebuild();
}

/* Back one mode; from the picker, back to the main menu. */
static void rightPressed(void)
{
    if (historyScreenMode)
    {
        historyScreenMode--;
        rebuild();
    }
    else
    {
        UG_WindowHide(&window_1);
        UG_WindowDelete(&window_1);
        currentScreen = &mainMenuScreen;
        currentScreen->init();
        currentScreen->render();
    }
}

/* Re-take the snapshot and keep the same record selected when it still exists. */
static void centerPressed(void)
{
    uint16_t oldSlot;
    uint32_t oldGeneration;

    if (!historyScreenMode)
    {
        return;
    }
    oldSlot = count ? slots[selected] : 0xFFFF;
    oldGeneration = count ? snapshotGeneration[oldSlot / HJ_PER_PAGE] : 0;
    snapshot();
    for (uint16_t i = 0; i < count; i++)
    {
        if (slots[i] == oldSlot && snapshotGeneration[oldSlot / HJ_PER_PAGE] == oldGeneration)
        {
            selected = i;
            break;
        }
    }
    render();
}

static void noAction(void)
{
}

/* Rendering per mode ======================================================= */

static void renderPicker(char *text, uint16_t size)
{
    uint8_t page = chosenDevice / ROWS_PER_PAGE * ROWS_PER_PAGE;
    uint8_t id;
    uint8_t a;
    uint8_t b;
    uint8_t exists;

    snprintf(text, size, "%u/5", page / ROWS_PER_PAGE + 1);
    UG_TextboxSetText(&window_1, 6, text);
    for (uint8_t row = 0; row < ROWS_PER_PAGE; row++)
    {
        id = page + row;
        a = 7 + row * 2;
        b = a + 1;
        exists = id < MAXIMUM_DEVICE && available(id);
        show(a, exists);
        show(b, exists);
        if (!exists)
        {
            continue;
        }
        snprintf(text, size, "M%u1%s", id + 1, deviceBuff[id].motorData[0].isHide ? " [Hidden]" : "");
        UG_TextboxSetText(&window_1, a, text);
        UG_TextboxSetText(&window_1, b, deviceBuff[id].isDelete ? "Removed device" : deviceBuff[id].motorData[0].name);
        for (uint8_t cell = a; cell <= b; cell++)
        {
            UG_TextboxSetBackColor(&window_1, cell, id == chosenDevice ? C_DIM_GRAY : C_BLACK);
            UG_TextboxSetForeColor(&window_1, cell, cell == b ? C_CYAN : C_WHITE);
            UG_TextboxSetScrollEnabled(&window_1, cell, id == chosenDevice);
        }
    }
}

static void renderList(char *text, uint16_t size)
{
    uint16_t index;
    uint8_t a;
    uint8_t b;
    const HistoryRecord *r;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    snprintf(text, size, "%u/%u", count ? selected / ROWS_PER_PAGE + 1 : 0, (count + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
    UG_TextboxSetText(&window_1, 6, text);
    for (uint8_t row = 0; row < ROWS_PER_PAGE; row++)
    {
        index = selected / ROWS_PER_PAGE * ROWS_PER_PAGE + row;
        a = 7 + row * 2;
        b = a + 1;
        r = view(index);
        show(a, index < count || (!count && !row));
        show(b, index < count);
        if (!r)
        {
            if (index < count)
            {
                UG_TextboxSetText(&window_1, a, "Record expired");
                UG_TextboxSetText(&window_1, b, "Hold CENTER: Refresh");
            }
            else if (!count && !row)
            {
                UG_TextboxSetText(&window_1, a, "No history yet");
            }
            continue;
        }

        /* Line 1: time and action; line 2: source and power transition. */
        Util_ConvertTimestampToRTC(r->event.timestamp, &time, &date);
        if (r->event.flags & HIST_TIME_UNKNOWN)
        {
            snprintf(text, size, "--:--:-- %s", actionName(r->event.action));
        }
        else
        {
            snprintf(text, size, "%02u:%02u:%02u %s", time.Hours, time.Minutes, time.Seconds,
                actionName(r->event.action));
        }
        UG_TextboxSetText(&window_1, a, text);
        snprintf(text, size, "%s %s > %s", r->event.source == HIST_MONITOR ? "Remote" : sourceName(r->event.source),
            powerName(r->event.before), powerName(r->event.after));
        UG_TextboxSetText(&window_1, b, text);
        for (uint8_t cell = a; cell <= b; cell++)
        {
            UG_TextboxSetBackColor(&window_1, cell, index == selected ? C_DIM_GRAY : C_BLACK);
            UG_TextboxSetForeColor(&window_1, cell, cell == b ? C_CYAN : r->event.error ? C_ORANGE : C_WHITE);
            UG_TextboxSetScrollEnabled(&window_1, cell, index == selected);
        }
    }
}

static void renderDetail(char *text, uint16_t size)
{
    char *labels[] = {"Action", "Source", "Before", "After", "Result", "Error"};
    char *values[DETAIL_FIELDS];
    const HistoryRecord *r;
    RTC_TimeTypeDef time;
    RTC_DateTypeDef date;

    snprintf(text, size, "%u/%u", count ? selected + 1 : 0, count);
    UG_TextboxSetText(&window_1, 6, text);
    r = view(selected);
    for (uint8_t i = 0; i < 8; i++)
    {
        show(7 + i, i < 7);
        UG_TextboxSetBackColor(&window_1, 7 + i, C_BLACK);
        UG_TextboxSetScrollEnabled(&window_1, 7 + i, 0);
    }
    if (!r)
    {
        UG_TextboxSetText(&window_1, 7, "Record expired");
        for (uint8_t i = 1; i < 7; i++)
        {
            UG_TextboxSetText(&window_1, 7 + i, "");
        }
        return;
    }

    /* Date/time line. */
    Util_ConvertTimestampToRTC(r->event.timestamp, &time, &date);
    if (r->event.flags & HIST_TIME_UNKNOWN)
    {
        strcpy(text, "Time not synced");
    }
    else
    {
        snprintf(text, size, "%02u/%02u/%02u %02u:%02u:%02u", date.Date, date.Month, date.Year,
            time.Hours, time.Minutes, time.Seconds);
    }
    UG_TextboxSetText(&window_1, 7, text);

    /* One "label: value" line per field. */
    values[0] = actionName(r->event.action);
    values[1] = sourceName(r->event.source);
    values[2] = r->event.before ? powerName(r->event.before) : "Unknown";
    values[3] = r->event.after ? powerName(r->event.after) : "Unknown";
    values[4] = resultName(r->event.result);
    values[5] = errorName(r->event.error);
    for (uint8_t i = 0; i < DETAIL_FIELDS; i++)
    {
        snprintf(text, size, "%s: %s", labels[i], values[i]);
        UG_TextboxSetText(&window_1, 8 + i, text);
        UG_TextboxSetForeColor(&window_1, 8 + i, i == 5 && r->event.error ? C_ORANGE : C_CYAN);
    }
}

/* Private helpers ========================================================== */

static void windowCallback(UG_MESSAGE *message)
{
    (void)message;
}

/* Record at a slot without validation; only used while sorting validated slots. */
static const HistoryRecord *raw(uint16_t slot)
{
    return (const HistoryRecord *)(HJ_BASE + (slot / HJ_PER_PAGE) * HJ_PAGE_SIZE +
        HJ_HEADER_SIZE + (slot % HJ_PER_PAGE) * 32u);
}

static uint32_t pageGeneration(uint16_t p)
{
    return *(const uint32_t *)(HJ_BASE + p * HJ_PAGE_SIZE);
}

/* A device is listed while paired, or while removed but still owning records. */
static uint8_t available(uint8_t id)
{
    uint16_t owner;
    const HistoryRecord *r;

    if (!deviceBuff[id].isDelete)
    {
        return 1;
    }
    owner = RemoteHistory_Owner(id);
    for (uint16_t s = 0; s < HJ_SLOTS; s++)
    {
        r = HJ_Get(s);
        if (r && r->owner == owner)
        {
            return 1;
        }
    }
    return 0;
}

/* Record at a snapshot index, or NULL if its page was rewritten since the snapshot. */
static const HistoryRecord *view(uint16_t index)
{
    if (index >= count ||
        snapshotGeneration[slots[index] / HJ_PER_PAGE] != pageGeneration(slots[index] / HJ_PER_PAGE))
    {
        return 0;
    }
    return HJ_Get(slots[index]);
}

static void snapshot(void)
{
    uint16_t owner;
    const HistoryRecord *r;
    const HistoryRecord *previous;
    uint16_t i;
    uint8_t newer;
    uint32_t a;
    uint32_t b;

    count = selected = 0;
    snapshotRevision = remoteHistoryRevision;
    for (uint16_t p = 0; p < HJ_PAGES; p++)
    {
        snapshotGeneration[p] = pageGeneration(p);
    }

    /* Insertion sort, newest first: by timestamp when both are known, else by write order. */
    owner = RemoteHistory_Owner(chosenDevice);
    for (uint16_t s = 0; s < HJ_SLOTS; s++)
    {
        r = HJ_Get(s);
        if (!r || r->owner != owner || r->event.serial != deviceBuff[chosenDevice].factoryInfo.serial)
        {
            continue;
        }
        i = count++;
        /* All records were validated once. Sorting only reads immutable flash in uiMutex. */
        while (i)
        {
            previous = raw(slots[i - 1]);
            if (!((previous->event.flags | r->event.flags) & HIST_TIME_UNKNOWN) &&
                r->event.timestamp != previous->event.timestamp)
            {
                newer = r->event.timestamp > previous->event.timestamp;
            }
            else
            {
                a = snapshotGeneration[s / HJ_PER_PAGE];
                b = snapshotGeneration[slots[i - 1] / HJ_PER_PAGE];
                newer = a > b || (a == b && s > slots[i - 1]);
            }
            if (!newer)
            {
                break;
            }
            slots[i] = slots[i - 1];
            i--;
        }
        slots[i] = s;
    }
}

static char *powerName(uint8_t state)
{
    return state == MOTOR_POWER_ON ? "ON" : state == MOTOR_POWER_OFF ? "OFF" : "?";
}

static char *actionName(uint8_t action)
{
    switch (action)
    {
        case HIST_ON:
            return "ON";
        case HIST_OFF:
            return "OFF";
        case HIST_ALARM:
            return "ALARM";
        case HIST_CLEAR:
            return "CLEAR";
        case HIST_LINK_LOST:
            return "NO LINK";
        case HIST_LINK_RESTORED:
            return "LINK OK";
        default:
            return "?";
    }
}

static char *sourceName(uint8_t source)
{
    switch (source)
    {
        case HIST_REMOTE:
            return "Remote";
        case HIST_IO1:
            return "Slave IO1";
        case HIST_IO2:
            return "Slave IO2";
        case HIST_IO3:
            return "Slave IO3";
        case HIST_IO4:
            return "Slave IO4";
        case HIST_TIMER:
            return "Timer";
        case HIST_MONITOR:
            return "Remote check";
        default:
            return "Slave";
    }
}

static char *errorName(uint16_t error)
{
    return error == HIST_ERROR_LOW_CURRENT ? "Low current" : error == HIST_ERROR_NO_STATUS ? "No STATUS" :
        error ? "Unknown" : "None";
}

static char *resultName(uint8_t result)
{
    return result == HIST_APPLIED ? "Applied" : result == HIST_UNCHANGED ? "No change" :
        result == HIST_REJECTED ? "Rejected" : "Warning";
}

static void createText(uint8_t id, int x, int y, int xe, int ye, char *text, UG_COLOR bg)
{
    UG_TextboxCreate(&window_1, boxes[id], id, x, y, xe, ye);
    UG_TextboxSetFont(&window_1, id, DEFAULT_FONT);
    UG_TextboxSetText(&window_1, id, text);
    UG_TextboxSetBackColor(&window_1, id, bg);
    UG_TextboxSetForeColor(&window_1, id, C_WHITE);
    UG_TextboxSetAlignment(&window_1, id, ALIGN_CENTER_LEFT);
    UG_TextboxSetScrollEnabled(&window_1, id, 0);
}

static void show(uint8_t id, uint8_t visible)
{
    if (visible && !UG_TextboxCheckVisible(&window_1, id))
    {
        UG_TextboxShow(&window_1, id);
    }
    if (!visible && UG_TextboxCheckVisible(&window_1, id))
    {
        UG_TextboxHide(&window_1, id);
    }
}

/* Recreate the window after a mode change (layout differs per mode). */
static void rebuild(void)
{
    UG_WindowHide(&window_1);
    UG_WindowDelete(&window_1);
    init();
    render();
}

/* Move the device selection (picker) or the record selection (list/detail). */
static void move(int8_t direction)
{
    int id;

    if (historyScreenMode == MODE_PICKER)
    {
        for (id = chosenDevice + direction; id >= 0 && id < MAXIMUM_DEVICE; id += direction)
        {
            if (available(id))
            {
                chosenDevice = id;
                break;
            }
        }
    }
    else if (count)
    {
        if (direction < 0 && selected)
        {
            selected--;
        }
        if (direction > 0 && selected + 1 < count)
        {
            selected++;
        }
    }
    render();
}
