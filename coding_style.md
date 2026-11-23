# Coding Style

## Phạm vi áp dụng

- Bắt buộc áp dụng tài liệu này cho mọi code tự viết trong project.
- Không áp dụng để sửa format của code mặc định do STM32CubeMX tạo, STM32 HAL/CMSIS, middleware ST hoặc các thư viện vendor khác cung cấp.
- Phần code tự viết bên trong `/* USER CODE BEGIN */` và `/* USER CODE END */` của file CubeMX vẫn phải tuân theo style này.
- Không sửa hoặc format lại vùng code generated nếu không thật sự cần thiết cho chức năng.

## Phạm vi và API

- Hàm hoặc biến chỉ được dùng trong cùng một file `.c` phải có `static`.
- Hàm nội bộ không dùng tiền tố module; dùng tên mô tả nhiệm vụ của nó, ví dụ `handleConfigConnection()` hoặc `validatePacket()`.
- Chỉ khai báo trong file `.h` những API thực sự được gọi từ file khác.
- Không export helper nội bộ qua header.
- Mọi API được gọi từ file khác phải có tiền tố đại diện module, theo mẫu `<Module>_<Verb><Noun>`.
  - Ví dụ: `Comm_ProcessRxData()`, `RemoteHistory_Handle()`, `MotorHistory_Tick()` và `CmdHandler_Schedule_Tick()`.
- API của `command_handlers.c` dùng tiền tố `CmdHandler_`.
- API của `schedule.c` dùng tiền tố `Schedule_`.
- Khi một hàm đổi từ nội bộ thành API liên-file, bỏ `static`, thêm tiền tố module và thêm declaration vào header trong cùng thay đổi.

```c
/* Public API: declared in communication_lora.h. */
LoraPackage_t Comm_CreateNewPackage(...);
uint8_t Comm_ParsePayloadReqGetStatus(...);

/* Private helper: declared and used only in communication_lora.c. */
static uint16_t generateTransactionID(void);
```

## Naming

- Hàm public: `<Module>_<Verb><Noun>`.
  - Ví dụ: `Comm_ProcessRxData`, `Comm_CreateReqGetStatus`.
- `Req` là viết tắt chuẩn của `Request`.
- Handler command dùng tên `handle<Command>` và luôn là `static`.
  - Ví dụ: `static void handleHistory(LoraPackage_t packet)`.
- Tên hàm dùng `lowerCamelCase` sau tiền tố module; kiểu dữ liệu, macro và enum giữ theo quy ước hiện có của project.
- Kiểu `struct` dùng mẫu `<Module>_<Name>_t`, ví dụ `Link_ProbeState_t`, `Schedule_RequestState_t`. Nếu struct đại diện cho chính module thì có thể dùng `<Module>_t`, ví dụ `Button_t`.
- Kiểu `enum` phải có tiền tố module và hậu tố `_e`; giá trị enum bắt đầu bằng `e` và lặp lại tên kiểu.

```c
typedef enum CmdHandler_TimerState_e
{
    eCmdHandler_TimerState_Idle,
    eCmdHandler_TimerState_Waiting
} CmdHandler_TimerState_e;
```

## Command handler

- `cmdTable` chỉ dispatch vào các `static handler` trong `command_handlers.c`.
- Không trỏ trực tiếp từ `cmdTable` đến logic của module khác.
- Handler là lớp tiếp nhận packet; nó kiểm tra/routing cơ bản rồi gọi hàm xử lý chuyên trách.

```c
static void handleHistory(LoraPackage_t packet);

const CmdMapping_t cmdTable[] = {
    {CMD_HISTORY, handleHistory},
    {0, NULL}
};

static void handleHistory(LoraPackage_t packet)
{
    if (packet.code != CMD_HISTORY) return;
    RemoteHistory_Handle(packet);
}
```

## Screen

- Mỗi file `screen_<xxx>.c` định nghĩa một `Screen_t`; các handler trong bảng là `static` và dùng tên chung giống nhau ở mọi screen: `init`, `render`, `leftClicked`, `rightClicked`, `centerClicked`, `bLeftClicked`, `bRightClicked`, `leftPressed`, `rightPressed`, `centerPressed`, `bLeftPressed`, `bRightPressed`.
- Handler không làm gì thì trỏ tới một hàm `static void noAction(void)` chung của file.
- Test giả lập gọi các hàm `static` này bằng tên kèm object file, ví dụ `symbol("screen_dss.o:init")`.

## Thứ tự trong file `.c`

Sắp xếp theo thứ tự sau:

1. `#include`
2. `#define` chỉ dùng nội bộ module
3. `extern` declaration
4. `static` variable và forward declaration của private function
5. Bảng dispatch hoặc dữ liệu module (`cmdTable`)
6. Public API của module
7. Private helper và handler implementation

- Macro/hằng số chỉ dùng trong một file `.c` phải đặt trong file `.c` đó, không đặt trong header. Chỉ đưa vào header các macro được file khác dùng hoặc cần cho khai báo trong header (ví dụ kích thước mảng của struct public).

- Các handler implementation phải cùng thứ tự với `cmdTable`.
- Gom các hàm cùng trách nhiệm vào một nhóm; dùng comment phân tách nhóm khi cần.
- Không đặt implementation lẫn vào giữa các khai báo, public API hoặc dispatch table.

## Thứ tự trong file `.h`

Sắp xếp theo thứ tự sau:

1. Include guard (`#ifndef MODULE_NAME_H` / `#define MODULE_NAME_H`)
2. `#include` chỉ những header cần cho các khai báo trong file này
3. Macro và hằng số (`#define`)
4. Kiểu dữ liệu: `enum`, `struct`, `typedef`
5. `extern` variable thực sự được dùng từ file khác
6. Khai báo public API
7. `#endif` của include guard

- Tên include guard viết hoa theo tên file, ví dụ `communication_lora.h` dùng `COMMUNICATION_LORA_H`.
- Khai báo public API theo cùng thứ tự với implementation trong file `.c`; gom theo nhóm trách nhiệm và dùng comment phân tách nhóm khi cần.
- Không khai báo hàm `static`, helper nội bộ hoặc hàm không có định nghĩa.
- Không đặt định nghĩa biến hoặc implementation hàm trong header (trừ macro và `static inline` khi thật sự cần).

```c
#ifndef COMMUNICATION_LORA_H
#define COMMUNICATION_LORA_H
#include <stdint.h>

#define LORA_PACKAGE_SIGNATURE 0xFF55

typedef struct LoraPackage_t
{
    uint16_t signature;
    uint8_t length;
} LoraPackage_t;

/* Package builders */
LoraPackage_t Comm_CreateNewPackage(...);

/* Receive path */
void Comm_ProcessRxData(uint8_t *buffer, uint32_t length);

#endif
```

## Format

- Dùng 4 spaces để thụt lề trong code mới; không dùng tab.
- Dấu mở ngoặc nhọn đặt trên dòng mới cho function definition và mọi cấu trúc điều khiển.
- Luôn dùng `{}` cho thân của `if`, `else`, `for`, `while` và `do`, kể cả khi chỉ có một câu lệnh.
- Có một dòng trống giữa các function.
- Không để trailing whitespace.
- Dùng `void` cho function không có tham số.
- Mỗi khai báo biến hoặc field của struct nằm trên một dòng riêng; không gộp nhiều biến trong một khai báo như `uint32_t a, b, c;`.
- Biến cục bộ của function khai báo ở đầu thân function, trước câu lệnh đầu tiên; không khai báo biến giữa function hoặc trong block `if`/`while`. Khi cần dùng thì gán giá trị tại chỗ.
  - Ngoại lệ: biến đếm của vòng `for` được khai báo trong câu lệnh `for`, ví dụ `for (uint8_t id = 0; ...)`.
- Trong function dài, tách các đoạn có nhiệm vụ khác nhau (kiểm tra đầu vào, chuẩn bị dữ liệu, xử lý, cập nhật trạng thái...) bằng một dòng trống.

```c
static void handleSetSchedule(LoraPackage_t fwPackage)
{
    PayloadCommonResponse_t payloadResponse;
    uint32_t deadline;
    uint8_t accepted;

    if (fwPackage.code != CMD_SET_SCHEDULE)
    {
        return;
    }

    memcpy(&payloadResponse, &fwPackage.rawData[LORA_PACKAGE_HEADER_SIZE], sizeof(payloadResponse));
    accepted = payloadResponse.result == 1;
}
```

```c
static uint8_t isValidPacket(const LoraPackage_t *packet)
{
    if (packet == NULL)
    {
        return FALSE;
    }

    return packet->signature == LORA_PACKAGE_SIGNATURE;
}
```

## Thay đổi code

- Khi đổi tên public API, cập nhật đồng thời definition, header và toàn bộ call site.
- Trước khi commit, kiểm tra không còn tham chiếu đến tên cũ và không có whitespace lỗi.
