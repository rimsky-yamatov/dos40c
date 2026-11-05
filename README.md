# DOS40C — MS-DOS 4.0 を C に移植して QEMU で動かす

[microsoft/MS-DOS](https://github.com/microsoft/MS-DOS) の v4.0 を参照して、
カーネルの中核を **C (GCC `-m16`)** で書き直したものです。リアルモードで
動く 1.44MB フロッピーイメージと、ブータブル ISO を生成します。

```
make all        # build/dos40c.img  build/dos40c.iso
make run        # QEMU でフロッピー起動
make run-iso    # QEMU で ISO(CD-ROM) 起動
make run-serial # ウィンドウ無し。端末がそのまま DOS のコンソールになる
```

必要なもの: `gcc` `binutils` `make` `python3`（ISO は外部ツール不要）、実行に `qemu-system-i386`。

## 何ができるか

- 独自ブートセクタ → 約 13KB のカーネル → `A:\>` プロンプト
- FAT12 の読み取り、`DIR` `TYPE` `VER` `VOL` `MEM` `ECHO` `CLS` `PAUSE` `HALT`、`.BAT` 実行
- `.COM` プログラムの実行と INT 21h サービス (`sample/hello.S`, `sample/dostest.S`)
- コンソールは VGA と COM1 の両方に出力 (`-serial stdio` で入力も可)

詳しい対応表と未実装項目は [docs/PORTING.md](docs/PORTING.md)。**これは MS-DOS 4.0 の
全機能の移植ではなく、起動して .COM が動く最小の中核**です (書き込み・サブディレクトリ・
.EXE・外部コマンドは未対応)。

## テスト

| コマンド | 内容 |
|---|---|
| `make test` | alloc / FAT12 / dir / handle をホスト上で 65 項目検証 |
| `make emu-test` | 自作の最小 386 リアルモードエミュレータ (`tests/emu`) でブート〜`HELLO.COM`/`DOSTEST.COM` 実行まで検証 |
| `make iso-test` | ISO9660 / El Torito を独立にパースして検証し、ISO 内イメージを起動 |
| `make smoke` | QEMU でセルフテストイメージを起動 (CI で実行) |

## CI / リリース

`.github/workflows/build.yml`: build → (QEMU smoke) → **ビルド成功ごとにリリース作成**
(`v4.0.<run番号>`, `dos40c.img` / `dos40c.iso` / `SHA256SUMS`)。smoke が失敗した場合は
pre-release として公開されます。

## 構成

```
boot/      ブートセクタ            kernel/   カーネル (C + 小さな asm)
tools/     mkfat, mkiso, smoke.sh  tests/    ホストテスト・エミュレータ
sample/    HELLO / DOSTEST / BAT   reference/ MS-DOS v4.0 原典 (無改変)
```

ライセンスは原典と同じ MIT。
