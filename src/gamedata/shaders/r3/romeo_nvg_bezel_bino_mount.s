function normal(shader, t_base, t_second, t_detail)
    shader:begin("romeo_nvg_bezel_bino_mount", "romeo_nvg_body")
        : sorting(2, true)
        : zb(true, true)
        : fog(false)
    shader:dx10texture("s_base", t_base)
    shader:dx10sampler("smp_base")
end
