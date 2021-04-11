from pygments.lexer import *
from pygments.token import *
from pygments.lexers.html import HtmlLexer

class PletLexer(RegexLexer):
    name = 'Plet'
    aliases = ['plet']
    filenames = ['*.tss']

    tokens = {
        'root': [
            include('command'),
        ],
        'command': [
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'#.*?\n', Comment.Single),
            (r'(if|then|else|for|in|switch|case|default|end|and|or|not|do|export|return|break|continue)\b', Keyword),
            (r'(true|false|nil)\b', Keyword.Constant),
            (r"'(\\\\|\\[^\\]|[^'\\])*'", String.Single),
            (r"\b[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?\b", Number),
            (r"\b[a-zA-Z_][a-zA-Z0-9_]*\b", Name),
            (r'\+|-|\*|/|<=|>=|==|!=|<|>|=|\?', Operator),
            (r'[\.,:|]', Punctuation),
            (r'\[', Punctuation, 'array'),
            (r'\(', Punctuation, 'expression'),
            (r'\{', Punctuation, 'object'),
            (r'\}', Comment.Preproc, 'template'),
            (r'"""', String, 'verbatim'),
            (r'"', String, 'string'),
            ('\s+', Whitespace),
        ],
        'template': [
            (r'[^{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, '#pop'),
        ],
        'string': [
            (r'[^"{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, 'statements'),
            ('"', String, '#pop'),
        ],
        'verbatim': [
            ('"""', String, '#pop'),
            (r'.', String),
        ],
        'statements': [
            (r'\}', Comment.Preproc, '#pop'),
            include('command'),
        ],
        'object': [
            (r'\}', Punctuation, '#pop'),
            include('command'),
        ],
        'array': [
            (r'\]', Punctuation, '#pop'),
            include('command'),
        ],
        'expression': [
            (r'\)', Punctuation, '#pop'),
            include('command'),
        ],
    }

class TxtPletLexer(PletLexer):
    name = 'TXT+Plet'
    aliases = ['txt+plet']
    filenames = ['*.plet.txt']

    tokens = {
        'root': [
            (r'[^{]+', Other),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, 'statements'),
        ],
    }

class HtmlPletLexer(DelegatingLexer):
    name = 'HTML+Plet'
    aliases = ['html+plet']
    filenames = ['*.plet.html']

    def __init__(self, **options):
        super().__init__(HtmlLexer, TxtPletLexer, **options)
