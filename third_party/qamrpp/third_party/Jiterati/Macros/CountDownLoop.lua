local M = ljiterati.macro.new("CountDownLoop")

M.main = function(counter)
    local loop_label = ljiterati.label.new("countdown.loop")
    local done_label = ljiterati.label.new("countdown.done")

    ljiterati.insn.label(loop_label)
    ljiterati.insn.load(counter)
    ljiterati.insn.const_i32(0)
    ljiterati.insn.cmp_eq()
    ljiterati.insn.jump_if(done_label)
    ljiterati.insn.dec(counter)
    ljiterati.insn.jump(loop_label)
    ljiterati.insn.label(done_label)
    ljiterati.insn.ret_void()
end

return M
