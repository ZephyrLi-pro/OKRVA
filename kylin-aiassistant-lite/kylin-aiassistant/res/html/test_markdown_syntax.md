# Markdown 语法测试文档

本文档包含常用的 Markdown 语法，用于测试 Markdown 解析和转换功能。

## 1. 标题 (Headings)

# 一级标题 (H1)
## 二级标题 (H2)
### 三级标题 (H3)
#### 四级标题 (H4)
##### 五级标题 (H5)
###### 六级标题 (H6)

## 2. 强调 (Emphasis)

**粗体文本** (Bold)
*斜体文本* (Italic)
***粗斜体*** (Bold Italic)
__下划线粗体__
_下划线斜体_
~~删除线~~ (Strikethrough)

## 3. 列表 (Lists)

### 无序列表

- 项目一
- 项目二
  - 子项目 2.1
  - 子项目 2.2
- 项目三

* 另一种无序列表
* 符号

### 有序列表

1. 第一项
2. 第二项
3. 第三项
   1. 嵌套有序 3.1
   2. 嵌套有序 3.2

### 任务列表

- [x] 已完成任务
- [x] 另一个已完成
- [ ] 未完成任务
- [ ] 另一个未完成

## 4. 链接 (Links)

[普通链接](https://www.example.com)
[带标题链接](https://www.example.com "点击访问 Example")
[相对链接](./another-file.md)

## 5. 图片 (Images)

![替代文本](https://via.placeholder.com/150 "图片标题")

## 6. 代码 (Code)

行内代码：使用 `code` 或 `` `code` `` 包裹

代码块（围栏式）：

```javascript
function hello() {
    console.log("Hello, Markdown!");
}
```

```python
def greet(name):
    return f"Hello, {name}!"
```

## 7. 引用 (Blockquotes)

> 这是一段引用文本。
> 可以多行显示。

> 嵌套引用
>> 二级引用
>>> 三级引用

## 8. 分隔线 (Horizontal Rules)

---

***

___

## 9. 表格 (Tables)

| 表头1 | 表头2 | 表头3 |
|:------|:-----:|------:|
| 左对齐 | 居中 | 右对齐 |
| 内容A | 内容B | 内容C |

## 10. 转义字符 (Escaping)

\* 显示星号 \*
\# 显示井号 \#
\[ 显示方括号 \]
\! 显示叹号 \!

## 11. HTML 标签 (HTML in Markdown)

<del>删除的文本</del>
<kbd>Ctrl</kbd> + <kbd>C</kbd>

## 12. 脚注 (Footnotes)

这是一段带脚注的文本[^1]。

[^1]: 这是脚注内容。

## 13. 定义列表 (Definition List)

术语一
: 定义一

术语二
: 定义二

## 14. 缩写 (Abbreviations)

*[HTML]: Hyper Text Markup Language
*[MD]: Markdown

## 15. 数学公式 (LaTeX - 部分渲染器支持)

行内公式：$E = mc^2$

块级公式：
$$
\sum_{i=1}^{n} i = \frac{n(n+1)}{2}
$$

## 16. 高亮 (Highlight - GFM 扩展)

==高亮文本==

## 17. 上标下标 (Subscript/Superscript - 部分支持)

H~2~O (水)
X^2^ (平方)

## 18. 复杂示例

**混合格式**：这是 `代码` 和 [链接](https://example.com) 以及 *斜体* 的组合。

> 引用中的 **粗体** 和 `行内代码`

| 功能 | 语法 | 示例 |
|------|------|------|
| 粗体 | `**text**` | **粗体** |
| 斜体 | `*text*` | *斜体* |
| 链接 | `[text](url)` | [链接](.) |
