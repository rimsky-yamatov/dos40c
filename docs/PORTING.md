# 移植対応表 (MS-DOS 4.0 → C)

原典は `reference/MS-DOS-v4.0/src/` に無改変で置いてある。

| 原典 (v4.0/src) | 移植先 | 内容 |
|---|---|---|
| `BOOT/MSBOOT.ASM` | `boot/boot.S` | FAT12 ブートセクタ。IO.SYS 探索の代わりに予約セクタからカーネルを読む |
| `BIOS/MSBIO*.ASM`, `MSCON/MSDISK/MSAUX` | `kernel/hal_bios.c` | INT 10h/13h/16h とシリアル(COM1)による HAL |
| `BIOS/SYSINIT*.ASM`, `DOS/MSINIT.ASM` | `kernel/entry.S`, `kernel/main.c` | 起動、.bss クリア、ベクタ設定 |
| `DOS/ALLOC.ASM` | `kernel/dos_alloc.c` | メモリアリーナ (AH=48h/49h/4Ah) |
| `DOS/FAT.ASM`, `DISK.ASM`, `BUF.ASM` | `kernel/dos_fat.c` | FAT12 マウント・クラスタチェーン・1 セクタキャッシュ |
| `DOS/DIR.ASM`, `SEARCH.ASM`, `PATH.ASM` | `kernel/dos_dir.c` | ルートディレクトリ、8.3 名、ワイルドカード検索 |
| `DOS/HANDLE.ASM`, `OPEN.ASM`, `CLOSE.ASM` | `kernel/dos_handle.c` | ファイルハンドル (SFT 相当) |
| `DOS/STDCPMIO.ASM`, `STRIN.ASM` | `kernel/dos_cpmio.c` | コンソール I/O、行編集 |
| `DOS/EXEC.ASM`, `ABORT.ASM` | `kernel/dos_exec.c`, `kernel/isr.S` | PSP 構築、.COM ロード、終了処理 |
| `DOS/DISPATCH.ASM`, `MSDISP.ASM` | `kernel/dos_dispatch.c`, `kernel/isr.S` | INT 20h/21h エントリとファンクション表 |
| `CMD/COMMAND/*` | `kernel/command.c` | 内蔵コマンド (DIR TYPE VER ...) とバッチ |

## 実装済み INT 21h

00 01 02 06 07 08 09 0A 0B 0C 0D 0E 19 1A 25 2F 30 33 35 36 3D 3E 3F 40(コンソールのみ)
42 47 48 49 4A 4C 4D 4E 4F 50 51 62

## 未実装 / 制限

- ディスクは読み取り専用 (作成・書き込み・削除・リネーム無し)
- ルートディレクトリのみ (サブディレクトリ無し)、ドライブは A: 固定、FAT12 のみ
- 実行形式は .COM のみ (.EXE リロケーション無し)、AH=4Bh (プログラム内からの EXEC) 無し
- 日付・時刻、FCB 系、IOCTL、ネットワーク/共有、コードページ/NLS 無し
- 原典の外部コマンド (FORMAT, CHKDSK, EDLIN ...) は未移植
