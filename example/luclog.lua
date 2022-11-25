--- clog的lua层
-- 简单的lua层模板,可直接使用,也可继承
-- @module luclog
-- @author <zhenghongwei@onemt.com.cn>
local next = next
local table = table
local debug = debug
local pairs = pairs
local string = string
local tostring = tostring
local string_format = string.format
local string_rep = string.rep

local skynet = require "skynet"
local clog = require("clog")
local _svcaddr = tostring(skynet.self())
--------------------------------------------------
local luclog = {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4,
    _dumpWrap = true, -- 输出时需要换行
}

--- 初始化日志,一般只在logger内做一次
-- @param ftime flush time
-- @param loggers 不同的logger的配置数组(max=1000)
-- @param logger.level 输出等级,默认DEBUG,大于等于时输出日志
-- @param logger.dir 文件输出目录,支持递归创建目录,nil则输出到控制台
-- @param logger.fname 文件的前缀名,fname.log.210313.0
-- @param logger.fsize 文件容量,单位MB
-- @param logger.cuthour 按小时切割文件,可选,1=true,0=false
-- @usage
-- Log.init(3, {
--     {dir = "./log1", fname = "test1", fsize = 200},
--     {level = Log.INFO, dir = "./log2", fname = "test2", fsize = 200},
--     {level = Log.DEBUG, dir = "./log3", fname = "test3", cuthour = 1},
-- })
function luclog.init(ftime, loggers)
    clog.init(ftime, loggers)
end

function luclog.exit()
    clog.exit()
end

--- 初始化日志输出到控制台
function luclog.initConsole()
    clog.init(0, {
        {
            level = Log.DEBUG,
        },
    })
end

function luclog.d(...)
    clog.output(luclog.DEBUG, luclog._logtag(), ...)
end

function luclog.i(...)
    clog.output(luclog.INFO, luclog._logtag(), ...)
end

function luclog.w(...)
    clog.output(luclog.WARN, luclog._logtag(), ...)
end

function luclog.e(...)
    clog.output(luclog.ERROR, luclog._logtag(), ...)
end

function luclog.f(...)
    clog.output(luclog.FATAL, luclog._logtag(), ...)
end

function luclog.dump(tbl, desc, nest)
    local msg = luclog.dumpstr(tbl, desc, nest)
    clog.output(luclog.DEBUG, luclog._logtag(), msg)
end

--- 获取dump字符串
function luclog.dumpstr(src, dsc, nest, deep)
    dsc = dsc or "dumpstr"
    if type(src) ~= "table" then
        return string_format("%s=%s", dsc, tostring(src))
    end

    local function _dump_val(val)
        local ty = type(val) -- nil, boolean, number, string, functio, userdata, thread 和 table
        local fmt = (ty == "boolean" or ty == "number" or ty == "nil") and "%s," or "%q,"
        return string_format(fmt, tostring(val))
    end

    local indent = luclog._dumpWrap and "  " or ""
    local function _dump_key(key, _deep)
        local fmt = type(key) == "string" and "%s[\"%s\"]" or "%s[%s]"
        return string_format(fmt, string_rep(indent, _deep), tostring(key))
    end

    deep = deep or 1
    nest = type(nest) == "number" and nest or 10
    local refs = {[src]=true}
    local function _iter_dump(_src, _dsc, _nest, _deep)
        if _nest == 0 then
            return "\"over nest\","
        end

        local lines = {
            string_format("%s%s", _dsc, "={"),
        }
        for key, val in pairs(_src) do
            if type(val) ~= "table" then
                lines[#lines + 1] = string_format("%s=%s", _dump_key(key, _deep), _dump_val(val))
            elseif refs[val] then
                lines[#lines + 1] = string_format("%s=\"ref%s\",", _dump_key(key, _deep), tostring(val))
            else
                refs[val] = true
                lines[#lines + 1] = _iter_dump(val, _dump_key(key, _deep), _nest - 1, _deep + 1)
            end
        end
        lines[#lines + 1] = string_format("%s%s", string_rep(indent, _deep - 1), _deep == 1 and "}" or "},")
        return luclog._dumpWrap and table.concat(lines, "\n") or table.concat(lines)
    end

    local _, ret = pcall(_iter_dump, src, dsc, nest, deep)
    return ret
end

function luclog.traceback(msg)
    local trace = debug.traceback(tostring(msg), 2)
    clog.output(luclog.DEBUG, luclog._logtag(), trace)
end

--- 定制日志标签
-- this is an example
-- you should overwrite this function
-- at least return ""
local _tokens, _tokenn = {}, 0
function luclog._logtag()
    local function _tagkv(k, v)
        if not k or not v then
            return
        end
        _tokenn = _tokenn + 1
        _tokens[_tokenn] = string_format("%s%s", k, v)
    end

    _tokenn = 0
    _tagkv("s:", _svcaddr)
    _tagkv("n:", SERVICE_NAME)
    _tokens[_tokenn + 1] = nil

    return table.concat(_tokens, "|") or ""
end

-- 格式输出到指定logger
function luclog.lawput(logidx, lv, ...)
    clog.lawput(logidx, lv, luclog._logtag(), ...)
end

-- 纯净输出到logger
function luclog.rawput(logidx, ...)
    clog.rawput(logidx, ...)
end

return luclog

