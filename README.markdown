ptmux: Terminal device multiplexer
----------------------------------
   
Each input char from the source terminal gets displayed either on a specific
pseudoterminal device if following a char in range [0...PT_COUNT] or on the
default pts otherwise. Input chars in range [0...PT_COUNT] are consumed 
internally and they're not forwarded to pseudoterminals.
   
Input from all pseudoterminals is collected and sent to the source terminal.
