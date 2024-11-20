local function main()
    local main_number = 3
    local main_string = "ghi"

    local function foo()
        local foo_number = 2
        local foo_string = "def"
    
        local function bar()
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
    
        bar()
    end

    foo()
end

require("./a/a")()

while true do
    main()
end