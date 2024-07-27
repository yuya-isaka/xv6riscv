//
// qemuのvirtioディスクデバイス用ドライバ。
// qemuのmmioインターフェイスを使用。
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "virtio.h"

// virtio mmioレジスタのアドレス。
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct disk {
  // DMAディスクリプタのセット。各ディスクリプタは個別のディスク操作の読み書き先をデバイスに指示。
  struct virtq_desc *desc;

  // ドライバが処理を希望するディスクリプタ番号を記録するリング。
  struct virtq_avail *avail;

  // デバイスが処理を完了したディスクリプタ番号を記録するリング。
  struct virtq_used *used;

  // 内部管理用。
  char free[NUM];  // ディスクリプタの空き状況
  uint16 used_idx; // usedリングの処理済み位置

  // 処理中の操作に関する情報を保持。
  struct {
    struct buf *b;
    char status;
  } info[NUM];

  // ディスクコマンドヘッダ。
  struct virtio_blk_req ops[NUM];

  struct spinlock vdisk_lock;

} disk;

void
virtio_disk_init(void)
{
  uint32 status = 0;

  initlock(&disk.vdisk_lock, "virtio_disk");

  if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R(VIRTIO_MMIO_VERSION) != 2 ||
     *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
     *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
    panic("could not find virtio disk");
  }

  // デバイスリセット
  *R(VIRTIO_MMIO_STATUS) = status;

  // ACKNOWLEDGEステータスビットを設定
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  // DRIVERステータスビットを設定
  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // 機能のネゴシエーション
  uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1 << VIRTIO_BLK_F_RO);
  features &= ~(1 << VIRTIO_BLK_F_SCSI);
  features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1 << VIRTIO_BLK_F_MQ);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  // 機能ネゴシエーションが完了したことをデバイスに通知
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // FEATURES_OKがセットされていることを確認
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // キュー0の初期化
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // キュー0が使用中でないことを確認
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // 最大キューサイズを確認
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // キューメモリの割り当てと初期化
  disk.desc = kalloc();
  disk.avail = kalloc();
  disk.used = kalloc();
  if(!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk kalloc");
  memset(disk.desc, 0, PGSIZE);
  memset(disk.avail, 0, PGSIZE);
  memset(disk.used, 0, PGSIZE);

  // キューサイズの設定
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // 物理アドレスの設定
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

  // キューが準備完了
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // 全てのディスクリプタを未使用に設定
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // デバイスが完全に準備完了したことを通知
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.cとtrap.cはVIRTIO0_IRQからの割り込みを設定する
}

// 空きディスクリプタを見つけ、使用中にマークし、そのインデックスを返す
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// ディスクリプタを未使用にマーク
static void
free_desc(int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

// ディスクリプタのチェーンを解放
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// 三つのディスクリプタを割り当てる（連続でなくてもよい）
// ディスク転送は常に三つのディスクリプタを使用
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

// ディスクの読み書きを行う
void
virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk.vdisk_lock);

  // 標準のSection 5.2によると、レガシーブロック操作は三つのディスクリプタを使用：
  // 一つはtype/reserved/sector用、一つはデータ用、一つは1バイトのステータス結果用

  // 三つのディスクリプタを割り当て
  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  // 三つのディスクリプタをフォーマット
  // qemuのvirtio-blk.cはこれらを読み込む

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // ディスクへの書き込み
  else
    buf0->type = VIRTIO_BLK_T_IN; // ディスクからの読み込み
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64) buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // デバイスがb->dataを読み取る
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // デバイスがb->dataに書き込む
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // デバイスは成功時に0を書き込む
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // デバイスがステータスを書き込む
  disk.desc[idx[2]].next = 0;

  // virtio_disk_intr()のためのstruct bufを記録
  b->disk = 1;
  disk.info[idx[0]].b = b;

  // ディスクリプタのチェーンの最初のインデックスをデバイスに通知
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // デバイスに新しいavailリングエントリがあることを通知
  disk.avail->idx += 1; // % NUMではない...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // 値はキュー番号

  // virtio_disk_intr()が要求の完了を通知するのを待つ
  while(b->disk == 1) {
    sleep(b, &disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release(&disk.vdisk_lock);
}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // デバイスはこの行で割り込みをクリアする
  // 新しいエントリがusedリングに書き込まれると、次の割り込みが発生する可能性があるが、
  // それは問題ない。
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // デバイスはusedリングにエントリを追加するたびにdisk.used->idxをインクリメントする

  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    struct buf *b = disk.info[id].b;
    b->disk = 0;   // ディスクはバッファを使い終わった
    wakeup(b);

    disk.used_idx += 1;
  }

  release(&disk.vdisk_lock);
}
