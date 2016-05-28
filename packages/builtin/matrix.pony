use "collections"
class Matrix[A, n: USize, dims: Vector[USize, #n] val]
  embed _data: Vector[A, # alloc()]

  fun tag alloc(): USize =>
    var i: USize = 0
    var acc: USize = 1
    while i < n do
      acc = acc * dims._apply(i = i + 1)
    end
    acc

  new undefined[B: (A & Real[B] val & Number) = A]() =>
    """
    Create a matrix of elements, populating them with random memory. This
    is only allowed for a vector of numbers.
    """
    _data = Vector[A, # alloc()]._create()

  new generate(f: {ref(USize): A^ ?} ref) ? =>
    """
    Create a matrix initiliased using a generator function
    """
    _data = Vector[A, # alloc()].generate(f)

  fun size(i: USize): USize ? =>
    dims(i)

  fun _calculate_address(indices: Vector[USize, #n]): USize ? =>
    if n == 0 then return 0 end
    var address: USize = 0
    var i: USize = 0
    while i < (n - 1) do
      if indices(i) > dims(i) then error end
      address = (address + indices(i)) * dims(i + 1)
      i = i + 1
    end
    if indices(i) > dims(i) then error end
    address + indices(i)
    
  fun apply(indices: Vector[USize, #n]): this->A ? =>
    _data._apply(_calculate_address(indices))

  fun ref update(indices: Vector[USize, #n], value: A): A^ ? =>
    _data._update(_calculate_address(indices), consume value)
