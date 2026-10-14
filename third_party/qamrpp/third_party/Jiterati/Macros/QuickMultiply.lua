local M = ljiterati.macro.new("QuickMultiply")

M.main = function(lhs, rhs)
    ljiterati.insn.load(lhs)
    ljiterati.insn.load(rhs)
    ljiterati.insn.mul()
    ljiterati.insn.ret()
end

return M
