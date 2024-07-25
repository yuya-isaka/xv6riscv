struct buf {
  int valid;   // データがディスクから読み込まれたかどうかを示すフラグである。
  int disk;    // ディスクがこのバッファを「所有」しているかどうかを示すフラグである。
  uint dev;    // デバイス番号である。
  uint blockno; // ブロック番号である。
  struct sleeplock lock; // バッファのスリープロックである。
  uint refcnt; // バッファの参照カウントである。
  struct buf *prev; // 最も最近使用された（LRU）キャッシュリストの前のバッファである。
  struct buf *next; // 最も最近使用された（LRU）キャッシュリストの次のバッファである。
  uchar data[BSIZE]; // バッファのデータである。
};
