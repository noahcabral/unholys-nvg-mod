function normal(shader, t_base, t_second, t_detail)
    shader:begin("ssfx_glass", "romeo_nvg_world_glass")
        : sorting(2, true)
        : zb(true, true)
        : fog(true)
    shader:dx10texture("s_env0", "$user$sky0")
    shader:dx10texture("s_env1", "$user$sky1")
    shader:dx10sampler("smp_rtlinear")
end
