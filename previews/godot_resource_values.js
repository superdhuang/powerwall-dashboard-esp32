export function numericValues(value) {
  // Vector3/Vector2/vec4 suffixes are type names, not the first coordinate.
  return (value.replace(/^[A-Za-z_]\w*\s*\(/,'').match(/[-+]?\d*\.?\d+(?:e[-+]?\d+)?/gi)||[]).map(Number);
}

export function transformRows(v) {
  // Verified against Godot3.2 str2var: Transform text serializes BASIS ROWS.
  return [v[0],v[1],v[2],v[9],v[3],v[4],v[5],v[10],v[6],v[7],v[8],v[11],0,0,0,1];
}
