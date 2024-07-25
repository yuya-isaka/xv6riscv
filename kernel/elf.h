// ELF実行ファイルのフォーマットである。

#define ELF_MAGIC 0x464C457FU  // リトルエンディアンで"\x7FELF"

// ファイルヘッダ
struct elfhdr {
  uint magic;  // ELF_MAGICと一致する必要がある。
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;    // プログラムエントリーポイントアドレス。
  uint64 phoff;    // プログラムヘッダテーブルのファイルオフセット。
  uint64 shoff;    // セクションヘッダテーブルのファイルオフセット。
  uint flags;
  ushort ehsize;   // ELFヘッダのサイズ。
  ushort phentsize;// プログラムヘッダエントリのサイズ。
  ushort phnum;    // プログラムヘッダエントリの数。
  ushort shentsize;// セクションヘッダエントリのサイズ。
  ushort shnum;    // セクションヘッダエントリの数。
  ushort shstrndx; // セクションヘッダ文字列テーブルインデックス。
};

// プログラムセクションヘッダ
struct proghdr {
  uint32 type;     // セクションの種類。
  uint32 flags;    // セクションのフラグ。
  uint64 off;      // セクションのファイルオフセット。
  uint64 vaddr;    // セクションの仮想アドレス。
  uint64 paddr;    // セクションの物理アドレス。
  uint64 filesz;   // セクションのファイルサイズ。
  uint64 memsz;    // セクションのメモリサイズ。
  uint64 align;    // セクションのアライメント。
};

// Proghdrのtypeフィールドの値
#define ELF_PROG_LOAD           1   // セクションがロード可能であることを示す。

// Proghdrのflagsフィールドのフラグビット
#define ELF_PROG_FLAG_EXEC      1   // セクションが実行可能であることを示す。
#define ELF_PROG_FLAG_WRITE     2   // セクションが書き込み可能であることを示す。
#define ELF_PROG_FLAG_READ      4   // セクションが読み取り可能であることを示す。
