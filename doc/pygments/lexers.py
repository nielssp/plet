from pygments.lexer import *
from pygments.token import *

class TscLexer(RegexLexer):
    name = 'TSC'
    aliases = ['tsc']
    filenames = ['*.tss']

    tokens = {
        'root': [
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'#.*?\n', Comment.Single),
            (r'(if|then|else|for|in|switch|case|default|end|and|or|not|do|export|return|break|continue)\b', Keyword),
            (r'(true|false|nil)\b', Keyword.Constant),
            (r"'(\\\\|\\[^\\]|[^'\\])*'", String.Single),
            (r"\b[0-9]+\b", Number),
            (r"\b[a-zA-Z_][a-zA-Z0-9_]*\b", Name),
            (r'\+|-|\*|/|<=|>=|==|!=|<|>|=', Operator),
            (r'[\.,:|]', Punctuation),
            (r'\[', Punctuation, 'array'),
            (r'\(', Punctuation, 'expression'),
            (r'\{', Punctuation, 'statements'),
            (r'\}', Punctuation, 'template'),
            (r'"', String, 'string'),
            ('\s+', Whitespace),
        ],
        'template': [
            (r'[^{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Text, '#pop'),
        ],
        'string': [
            (r'[^"{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Punctuation, 'statements'),
            ('"', String, '#pop'),
        ],
        'statements': [
            (r'\}', Punctuation, '#pop'),
            include('root'),
        ],
        'array': [
            (r'\]', Punctuation, '#pop'),
            include('root'),
        ],
        'expression': [
            (r'\)', Punctuation, '#pop'),
            include('root'),
        ],
    }

class TscTemplateLexer(RegexLexer):
    name = 'TSC Template'
    aliases = ['tsc-template']
    filenames = ['*.html.tss']

    tokens = {
        'root': [
            (r'[^{]+', Other),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Text, 'statements'),
        ],
        'statements': [
            (r'(.+?)(\})',
                bygroups(using(TscLexer), Name.Text),
                '#pop'),
        ],
    }
