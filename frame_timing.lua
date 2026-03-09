-- Frame timing observer for mpv
-- 计算视频实际渲染帧率

local max_samples = 60
local frame_times = {}
local last_timing = "0/0/0/0"

-- 使用定时器定期更新帧时间
mp.add_periodic_timer(0.1, function()
    -- 尝试多个属性获取帧率
    local fps = 0
    
    -- 方法1: 视频参数帧率
    local vfr = mp.get_property_number("video-params/frame-rate", 0)
    if vfr > 0 then
        fps = vfr
    end
    
    -- 方法2: 估计的视频滤镜帧率
    if fps <= 0 then
        local evf = mp.get_property_number("estimated-vf-fps", 0)
        if evf > 0 then
            fps = evf
        end
    end
    
    -- 方法3: 从视频轨道信息获取
    if fps <= 0 then
        local track_list = mp.get_property_native("track-list", {})
        for _, track in ipairs(track_list) do
            if track.type == "video" and track.fps then
                fps = tonumber(track.fps)
                break
            end
        end
    end
    
    -- 方法4: 使用display-fps作为后备
    if fps <= 0 then
        fps = mp.get_property_number("display-fps", 60)
    end
    
    if fps > 0 then
        local frame_us = 1000000 / fps
        
        table.insert(frame_times, frame_us)
        if #frame_times > max_samples then
            table.remove(frame_times, 1)
        end
        
        if #frame_times >= 2 then
            local sum = 0
            local peak = 0
            for i = 1, #frame_times do
                local v = frame_times[i]
                sum = sum + v
                if v > peak then peak = v end
            end
            local avg = sum / #frame_times
            local last = frame_times[#frame_times]
            last_timing = string.format("%.0f/%.0f/%.0f", last, avg, peak)
            mp.msg.info("Frame timing: " .. last_timing)
        end
    end
    
    mp.set_property("user-data/frame-timing", last_timing)
end)

mp.register_event("start-file", function()
    frame_times = {}
    last_timing = "0/0/0/0"
    mp.set_property("user-data/frame-timing", last_timing)
    mp.msg.info("start-file event: reset frame timing")
end)

mp.msg.info("Frame timing script loaded, version 2")
