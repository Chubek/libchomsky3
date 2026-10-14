local M = ljiterati.macro.new("QuickAdd")

M.main = function(lhs, rhs)
    ljiterati.insn.load(lhs)
    ljiterati.insn.load(rhs)
    ljiterati.insn.add()
    ljiterati.insn.ret()
end

return M
