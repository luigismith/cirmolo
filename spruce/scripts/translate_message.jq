# Cirmolo: translation of the messages that scripts show on screen.
#
# Input:    a PyUI language file (App/PyUI/lang/<Language>.json)
# Argument: --arg t "<English text as the script wrote it>"
# Output:   the text in that language, or $t unchanged when there is no entry.
#
# Same rules as Language.translate() in App/PyUI/main-ui/menus/language/language.py:
# an exact match in "scriptMessages" first; otherwise the longest entry with {placeholders}
# whose pattern matches the whole text. Captured values are translated as well when they
# are entries themselves. Used by translate_message() in spruce/scripts/helperFunctions.sh.

def rx_escape: gsub("(?<c>[\\\\^$.|?*+()\\[\\]{}])"; "\\\(.c)");

(.scriptMessages // {}) as $m
| if ($m | type) != "object" then $t
  elif ($m[$t] | type) == "string" then $m[$t]
  else
    first(
      $m | to_entries
      | map(select((.key | contains("{")) and (.value | type) == "string"))
      | sort_by(-(.key | length))
      | .[] as $e
      | ($e.key | rx_escape | gsub("\\\\\\{(?<n>[A-Za-z_][A-Za-z0-9_]*)\\\\\\}"; "(?<\(.n)>.*?)")) as $rx
      | ($t | capture("\\A" + $rx + "\\z"; "p")?) // empty
      | reduce to_entries[] as $c ($e.value; gsub("\\{" + $c.key + "\\}"; ($m[$c.value] // $c.value)))
    ) // $t
  end
