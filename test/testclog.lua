--- 测试luclog接口
-- @module testclog
-- @author <zhenghongwei@onemt.com.cn>
-- @usage log testclog api
-- @usage log testclog press 10

local skynet = require "skynet"
local Log = require("luclog")

local mode, cd = ...
mode = mode or "api"
cd = tonumber(cd or 10)

math.randomseed(tostring(os.time()):reverse():sub(1, 5))

-- 把time返回的数值字串倒过来低位变高位,再取高位5位
local function randomStr(len, str)
    local chars = {}
    for _=1,len do
        str = str or string.char(math.random(0,25)+65)
        chars[#chars + 1] = str
    end
    return table.concat(chars)
end

skynet.start(function()
    skynet.fork(function ()
        if mode == "api" then
            Log.d("测试debug", nil, {}, Log.DEDBUG, true, 123, nil)
            Log.i("测试info", nil, {}, Log.INFO, false, 123, nil)
            Log.w("测试warn", nil, {}, Log.WARN, true, 123, nil)
            Log.e("测试error", nil, {}, Log.ERROR, true, 123, nil)
            Log.f("测试fatal", nil, {}, Log.FATAL, false, 123, nil)

            local maxlog = randomStr(5*1024, string.char(65))
            Log.i("测试超长1", maxlog)
            Log.rawput(2, "测试超长2", maxlog, Log.INFO, {}, Log.DEDBUG, true, 123, nil)

            local dumptb = {123, "123", ["123"] = {123, "123", false}}
            Log.dump(dumptb, "测试dump", 5)
            Log.traceback("测试 traceback")

            Log.rawput(2, Log.INFO, "logger2", nil, {}, Log.DEDBUG, true, 123, nil)
        elseif mode == "press" then
            local count = 1
            while true do
                Log.i("测试info", nil, {}, Log.INFO, false, count, nil)
                Log.rawput(2, Log.INFO, "logger2", nil, {}, true, 123, os.time())
                count = count + 1
                skynet.sleep(cd)
            end
        end
    end)
end)


