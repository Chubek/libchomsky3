local M = ljiterati.macro.new("QuickDivide")

M.main = function(lhs, rhs)
    ljiterati.insn.load(lhs)
    ljiterati.insn.load(rhs)
    ljiterati.insn.div()
    ljiterati.insn.ret()
end

return M
