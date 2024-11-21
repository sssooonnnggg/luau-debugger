local function main()
    local main_number = 3
    local main_string = "ghi"

    local function foo()
        local foo_number = 2
        local foo_string = "def"
    
        local function bar(a, b)
            local bar_nil
            local bar_boolean = true
            local bar_number = 1
            local bar_vector = vector.create(100, 200, 300)
            local bar_string = "abc"
            local bar_table = {
                a = 1,
                b = 2,
            }
            local bar_array = {1, 2, 3, 4, 5}
            local bar_nested_table = {
                a = 1,
                b = 2,
                c = {
                    d = 3,
                    e = 4,
                    f = {
                        g = 5,
                        h = 6,
                    }
                }
            }
            local table_with_circle = {
                a = 1,
                b = {
                    c = 2,
                },
                e = {
                    f = 3
                }
            }
            table_with_circle.b.d = table_with_circle
            table_with_circle.e.g = table_with_circle.b
            local bar_function = function() end
            print("Hello, World!")
            print(foo_string)
        end
    
        bar(500, 600)
    end

    foo()
end

require("./a/a")()

local function foo()
  local a = 1
  local b = 2
end

local function bar()
  local c = 3
  local d = 4
end

local function test_step()
  foo()
  bar()
  foo()
  bar()
end

test_step()

local function test_loadstring()
  local a = 1
  local f = loadstring("print('loadstring==================', a)")
  local r, msg = pcall(f)
  if r == false then
    print('===========================', msg)
  end
end

test_loadstring()

while true do
    main()
end