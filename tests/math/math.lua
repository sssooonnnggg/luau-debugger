local function add(a, b)
  return a + b
end

local function sub(a, b)
  return a - b
end

local function mul(a, b)
  return a * b
end

local function div(a, b)
  return a / b
end

return {
  add = add,
  sub = sub,
  mul = mul,
  div = div,
}