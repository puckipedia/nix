let
  func = (a: a);
in
  func != func && { inherit func; } != { inherit func; } && [ func ] != [ func ] && !builtins.elem func [ func ]
