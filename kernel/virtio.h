//
// virtio デバイス定義。
// mmio インターフェイスと virtio ディスクリプタの両方に対応。
// qemu でのみテスト済み。
//
// virtio の仕様書:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio 制御レジスタは 0x10001000 からマッピングされる。
// qemu virtio_mmio.h より
#define VIRTIO_MMIO_MAGIC_VALUE        0x000 // マジック値: 0x74726976
#define VIRTIO_MMIO_VERSION            0x004 // バージョン; 2 であるべき
#define VIRTIO_MMIO_DEVICE_ID          0x008 // デバイスタイプ; 1 はネットワーク, 2 はディスク
#define VIRTIO_MMIO_VENDOR_ID          0x00c // ベンダーID: 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES    0x010 // デバイス機能
#define VIRTIO_MMIO_DRIVER_FEATURES    0x020 // ドライバ機能
#define VIRTIO_MMIO_QUEUE_SEL          0x030 // キューの選択, 書き込み専用
#define VIRTIO_MMIO_QUEUE_NUM_MAX      0x034 // 現在のキューの最大サイズ, 読み取り専用
#define VIRTIO_MMIO_QUEUE_NUM          0x038 // 現在のキューのサイズ, 書き込み専用
#define VIRTIO_MMIO_QUEUE_READY        0x044 // レディビット
#define VIRTIO_MMIO_QUEUE_NOTIFY       0x050 // 書き込み専用
#define VIRTIO_MMIO_INTERRUPT_STATUS   0x060 // 読み取り専用
#define VIRTIO_MMIO_INTERRUPT_ACK      0x064 // 書き込み専用
#define VIRTIO_MMIO_STATUS             0x070 // 読み書き
#define VIRTIO_MMIO_QUEUE_DESC_LOW     0x080 // ディスクリプタテーブルの物理アドレス, 書き込み専用
#define VIRTIO_MMIO_QUEUE_DESC_HIGH    0x084 // 上位アドレス
#define VIRTIO_MMIO_DRIVER_DESC_LOW    0x090 // アベイラブルリングの物理アドレス, 書き込み専用
#define VIRTIO_MMIO_DRIVER_DESC_HIGH   0x094 // 上位アドレス
#define VIRTIO_MMIO_DEVICE_DESC_LOW    0x0a0 // ユーズドリングの物理アドレス, 書き込み専用
#define VIRTIO_MMIO_DEVICE_DESC_HIGH   0x0a4 // 上位アドレス

// ステータスレジスタビット, qemu virtio_config.h より
#define VIRTIO_CONFIG_S_ACKNOWLEDGE    1 // ACKビット
#define VIRTIO_CONFIG_S_DRIVER         2 // ドライバビット
#define VIRTIO_CONFIG_S_DRIVER_OK      4 // ドライバOKビット
#define VIRTIO_CONFIG_S_FEATURES_OK    8 // 機能OKビット

// デバイス機能ビット
#define VIRTIO_BLK_F_RO              5  // ディスクは読み取り専用
#define VIRTIO_BLK_F_SCSI            7  // SCSIコマンドパススルーをサポート
#define VIRTIO_BLK_F_CONFIG_WCE     11  // コンフィグでライトバックモードを利用可能
#define VIRTIO_BLK_F_MQ             12  // 複数のキューをサポート
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// この数の virtio ディスクリプタが必要。
// 2 の累乗でなければならない。
#define NUM 8

// 仕様書に基づく単一のディスクリプタ。
struct virtq_desc {
  uint64 addr;  // ディスクリプタのアドレス
  uint32 len;   // ディスクリプタの長さ
  uint16 flags; // フラグ
  uint16 next;  // 次のディスクリプタへのリンク
};
#define VRING_DESC_F_NEXT  1 // 他のディスクリプタとチェーン
#define VRING_DESC_F_WRITE 2 // デバイスが書き込み（vs 読み取り）

// 仕様書に基づくアベイラブルリング全体。
struct virtq_avail {
  uint16 flags; // 常にゼロ
  uint16 idx;   // ドライバが次に書き込む ring[idx]
  uint16 ring[NUM]; // チェーンヘッドのディスクリプタ番号
  uint16 unused;
};

// "ユーズド" リングの1エントリで、デバイスが完了したリクエストをドライバに通知する。
struct virtq_used_elem {
  uint32 id;   // 完了したディスクリプタチェーンの開始インデックス
  uint32 len;
};

struct virtq_used {
  uint16 flags; // 常にゼロ
  uint16 idx;   // デバイスが ring[] エントリを追加するときにインクリメント
  struct virtq_used_elem ring[NUM];
};

// これらは特定の virtio ブロックデバイス（例: ディスク）に関連するもので、
// 仕様書のセクション5.2に記載されている。

#define VIRTIO_BLK_T_IN  0 // ディスクの読み取り
#define VIRTIO_BLK_T_OUT 1 // ディスクの書き込み

// ディスクリクエスト内の最初のディスクリプタの形式。
// これに続いてブロックを含む2つのディスクリプタと1バイトのステータスが続く。
struct virtio_blk_req {
  uint32 type; // VIRTIO_BLK_T_INまたは..._OUT
  uint32 reserved;
  uint64 sector;
};
