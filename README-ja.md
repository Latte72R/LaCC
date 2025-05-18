[English](README.md) | 日本語

![](./LaCC.png)

LaCC は、C コンパイラの仕様やメモリ構造の理解を目的として作成された、必要最小限のコア機能だけを実装したシンプルな C コンパイラです。

[![DeepWiki](https://img.shields.io/badge/DeepWiki-Latte72R%2FLaCC-blue.svg?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACwAAAAyCAYAAAAnWDnqAAAAAXNSR0IArs4c6QAAA05JREFUaEPtmUtyEzEQhtWTQyQLHNak2AB7ZnyXZMEjXMGeK/AIi+QuHrMnbChYY7MIh8g01fJoopFb0uhhEqqcbWTp06/uv1saEDv4O3n3dV60RfP947Mm9/SQc0ICFQgzfc4CYZoTPAswgSJCCUJUnAAoRHOAUOcATwbmVLWdGoH//PB8mnKqScAhsD0kYP3j/Yt5LPQe2KvcXmGvRHcDnpxfL2zOYJ1mFwrryWTz0advv1Ut4CJgf5uhDuDj5eUcAUoahrdY/56ebRWeraTjMt/00Sh3UDtjgHtQNHwcRGOC98BJEAEymycmYcWwOprTgcB6VZ5JK5TAJ+fXGLBm3FDAmn6oPPjR4rKCAoJCal2eAiQp2x0vxTPB3ALO2CRkwmDy5WohzBDwSEFKRwPbknEggCPB/imwrycgxX2NzoMCHhPkDwqYMr9tRcP5qNrMZHkVnOjRMWwLCcr8ohBVb1OMjxLwGCvjTikrsBOiA6fNyCrm8V1rP93iVPpwaE+gO0SsWmPiXB+jikdf6SizrT5qKasx5j8ABbHpFTx+vFXp9EnYQmLx02h1QTTrl6eDqxLnGjporxl3NL3agEvXdT0WmEost648sQOYAeJS9Q7bfUVoMGnjo4AZdUMQku50McDcMWcBPvr0SzbTAFDfvJqwLzgxwATnCgnp4wDl6Aa+Ax283gghmj+vj7feE2KBBRMW3FzOpLOADl0Isb5587h/U4gGvkt5v60Z1VLG8BhYjbzRwyQZemwAd6cCR5/XFWLYZRIMpX39AR0tjaGGiGzLVyhse5C9RKC6ai42ppWPKiBagOvaYk8lO7DajerabOZP46Lby5wKjw1HCRx7p9sVMOWGzb/vA1hwiWc6jm3MvQDTogQkiqIhJV0nBQBTU+3okKCFDy9WwferkHjtxib7t3xIUQtHxnIwtx4mpg26/HfwVNVDb4oI9RHmx5WGelRVlrtiw43zboCLaxv46AZeB3IlTkwouebTr1y2NjSpHz68WNFjHvupy3q8TFn3Hos2IAk4Ju5dCo8B3wP7VPr/FGaKiG+T+v+TQqIrOqMTL1VdWV1DdmcbO8KXBz6esmYWYKPwDL5b5FA1a0hwapHiom0r/cKaoqr+27/XcrS5UwSMbQAAAABJRU5ErkJggg==)](https://deepwiki.com/Latte72R/LaCC)

## サポートされている機能

### 1. データ型

* **プリミティブ型**: `int`, `char`, `void`
* **派生型**: ポインタ (`T*`), 配列 (`T[]`)
* **複合型**: 構造体 (`struct`), 列挙体 (`enum`)

### 2. 関数

* **定義:**  
  パラメータと戻り値の型を指定できます
* **宣言と呼び出し:**  
  関数の定義, 呼び出し, `return` での値を返却が可能です

### 3. グローバル＆ローカル変数

グローバル変数とローカル（スタック上）変数の宣言がサポートされています. 

### 4. 制御構造

* **条件分岐**

  * `if (condition) { … }`
  * `else { … }`
* **ループ**

  * `for (init; condition; step) { … }`  
    `init`, `condition`, `step` の省略も可能です
  * `while (condition) { … }`
  * `do { … } while (condition);`
* **ループ制御**

  * `break`
  * `continue`

### 5. 演算子

* **算術**: `+`, `-`, `*`, `/`, `%`
* **比較**: `==`, `!=`, `<`, `<=`, `>`, `>=`
* **論理**: `&&`, `||`, `!`
* **ビット演算**: `&`, `|`, `^`, `~`, `<<`, `>>`

### 6. その他

* **インクルード指令**（制限あり）
  ダブルクォート形式の `#include "lacc.h"` は処理できますが, `<stdio.h>` など標準ライブラリヘッダはサポートしていません. 

* **配列の初期化リスト**（制限あり）

  1. 数値リテラルのみを使った初期化:

     ```c
     int arr[3] = {3, 6, 2};
     ```
  2. 文字配列への文字列リテラル初期化:

     ```c
     char str[15] = "Hello, World!\n";
     ```

  ネストした初期化リストはサポートしていません. 

* **extern 宣言**
  基本型, ポインタ, 配列などの外部変数宣言が可能です. 

* **typedef サポート**（制限あり）
  型エイリアスを作る `typedef` が使えます. 

* **構造体メンバアクセス**
  ドット (`.`) とアロー (`->`) の両方に対応しています. 

* **2進数・16進数リテラル**
  例） `0b001011`,  `0xFF2A`

* **コメント**

  ```c
  // 1行コメント
  /* 
     複数行コメント 
  */
  ```

## サポート外の構文

以下はサポートしていません:

* ネストした関数定義
* `switch` / `case` / `default`
* `goto` とラベル
* 三項演算子 (`?:`)
* `union` 型
* `unsigned`, `long`, `float`, `double` などの拡張プリミティブ型
* `const`, `volatile`, `static`, `register`, `auto` などの型修飾子・ストレージ指定子
* 構造体の初期化リスト（例: `struct AB p = {.a = 1, .b = 2};`）
* インラインアセンブリ
* プリプロセッサディレクティブ（`#define`, `#ifdef` など）
* 可変長引数関数 (`...`) の定義
* カンマ演算子
* 関数ポインタ
* 明示的な型キャスト
* 可変長配列 (VLA)

## 制限事項

### 単一ユニットコンパイル

単一の `.c` ファイルしか扱えず, 複数翻訳単位の分割コンパイルやリンクはできません.   
それぞれの `.c` ファイルを LaCC によりコンパイルした後に,  `clang` や `gcc` によってリンクしてください. 

### 最適化

コード生成における最適化は一切行われません. 

## LaCC の使い方

### 1. リポジトリのクローン

```bash
git clone https://github.com/Latte72R/LaCC
cd LaCC
```

### 2. コンパイラのビルド

```bash
make
```

これで `lacc` バイナリが生成されます.   
変更がなければ, 

```bash
'lacc' is up to date.
```

と表示されます. 

### 3. セルフホストコンパイラのビルド

```bash
make laccs
```

`lacc` で自身のソースを再コンパイルし, 第二段階（セルフホスト）コンパイラ `laccs` を生成します. 

### 4. 基本テストの実行

```bash
make test
```

内部では, 

```bash
./multitest.sh ./lacc
```

を実行し, 第一段階コンパイラ `lacc` のテストを行います. 

### 5. セルフホストテストの実行

```bash
make selfhost-test
```

内部では, 

```bash
./multitest.sh ./laccs
```

を実行し, 第二段階（セルフホスト）コンパイラ `laccs` のテストを行います. 

### 6. クリーンアップ

```bash
make clean
```

`lacc`, `laccs`, オブジェクトファイル(`*.o`), アセンブリファイル(`*.s`), 一時ファイル(`tmp*`)などを削除して, クリーンアップします. 

## 作者について

LaCC は学生エンジニア **Latte72** が設計・開発しています！

### リンク

* **Web サイト:** [https://latte72.net/](https://latte72.net/)
* **GitHub:** [https://github.com/latte72r/LaCC](https://github.com/latte72r)
* **X (a.k.a Twitter):** [https://twitter.com/Latte72R](https://twitter.com/Latte72R)
* **Qiita:** [https://qiita.com/latte72r](https://qiita.com/latte72r)
