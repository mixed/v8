// Copyright 2014 the V8 project authors. All rights reserved.
// AUTO-GENERATED BY tools/generate-runtime-tests.py, DO NOT MODIFY
// Flags: --allow-natives-syntax --harmony
var _parent_wrapper = new String('foo');
var _orig_wrapper = new String('foo');
var _subst_wrapper = new String('foo');
try {
%LiveEditReplaceRefToNestedFunction(_parent_wrapper, _orig_wrapper, _subst_wrapper);
} catch(e) {}
