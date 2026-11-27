function normal(shader, t_base, t_second, t_detail)
    shader:begin("ssfx_glass", "romeo_nvg_well")
        : sorting(2, true)
        : zb(true, true)
        : fog(false)
end
