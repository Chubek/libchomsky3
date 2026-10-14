local M = ljiterati.macro.new("QuickSubtract")

M.main = function(lhs, rhs)
    ljiterati.insn.load(lhs)
    ljiterati.insn.load(rhs)
    ljiterati.insn.sub()
    ljiterati.insn.ret()
end

return M
