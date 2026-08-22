## Bery Lexical Analysis

This readme is intended to give you sufficient knowledge about Bery's Lexical analysis a.k.a Lexer. Bery's lexer follows simple modular structure having different method/function for different scanning tasks.


#### Maximal Munch rule
Lexer follows maximal munch trick to emit the correct tokens in a single pass. 

e.g. if lexer finds '+' then it look ahead for next symbol, if it is '+' it emits `TOKEN_INC`, if it is '=' it emits `TOKEN_PLUS_EQUAL`, if it is neither then it emits `TOKEN_PLUS`.

---


#### Whitespaces & Comments rule
Lexer intentionally drops every whitespaces (' ', \t, \n, \r), and every comment block (--- Single line comment) or (--! multi line comment block !--) and only emits the token the langauge supports.



#### Tokenization rule

|Category | Rule |
|---|---|
|Whitepsaces | Ignored, except for line counting|
|Comments| Ignored|
|Keywords|Recognised after identifier scanning|
|Identifiers|Letter or _ followed by letters, digits or _|
|Operators| Maximal munch|


#### Keywords 
| | | | | | |
|--|--|--|--|--|--|
|int|float|bigint|double|bool|char|
|string|run|true|false|null|if|
|else|switch|case|default|break|continue|
|pass|while|do|for|in|func|
|return|enum|import|extern|class|attributes|
|methods|new|public|private|protected|ref|
|super||||||


#### Complexity analysis

- Scanning : Single pass
- Token Recognization : Maximal Munch
- Time complexity : O(n)
- Keyword lookup : O(1)
- Memory Complexity : O(tokens)


#### Error Handling

`yet to be added`