=begin
Bitmap Drawing Extensions
-------------------------
These are some bitmap extensions to be used along with MKXP-Z.

The old Ruby was slow as snails, but with MKXP-Z's new access to raw data, 
things like these are possible (I mean they're possible but... anyway).

I also implemented the good old alpha blending algorithm for colors, 
since it's useful for drawing things over other things.

=end
# Simple function used in drawCircle to convert colors to integers
class Color
  # Byte order seems to be ARGB instead of RGBA.
  def to_i
    return self.alpha.to_i << 24 | self.red.to_i << 16 | self.green.to_i << 8 | self.blue.to_i
  end
  
  # Returns a color from an integer value.
  def from_i(value)
    self.alpha = (value & 0b11111111000000000000000000000000) >> 24
    self.red   = (value & 0b00000000111111110000000000000000) >> 16
    self.green = (value & 0b00000000000000001111111100000000) >> 8
    self.blue  = (value & 0b00000000000000000000000011111111)
  end
  
  # Simple alpha blending algorithm.
  def blend(bgCol)
    # Separate the R, G, B, and A values for BG and FG
    red_bg = bgCol.red
    green_bg = bgCol.green
    blue_bg = bgCol.blue
    alpha_bg = bgCol.alpha / 255.0
    red_fg = self.red
    green_fg = self.green
    blue_fg = self.blue
    alpha_fg = self.alpha / 255.0
    # Calculate the alpha of the new, final colour
    alpha_final = alpha_bg + alpha_fg - alpha_bg * alpha_fg
    # Pre-multiply all the R, G, and B values by their alpha value
    red_bg_a = red_bg * alpha_bg
    green_bg_a = green_bg * alpha_bg
    blue_bg_a = blue_bg * alpha_bg
    red_fg_a = red_fg * alpha_fg
    green_fg_a = green_fg * alpha_fg
    blue_fg_a = blue_fg * alpha_fg
    # Calculate the new, final R, G, and B values
    red_final_a = red_fg_a + red_bg_a * (1 - alpha_fg)
    green_final_a = green_fg_a + green_bg_a * (1 - alpha_fg)
    blue_final_a = blue_fg_a + blue_bg_a * (1 - alpha_fg)
    # Un-multiply the final R, G, and B values by the final alpha
    red_final = red_final_a / alpha_final
    green_final = green_final_a / alpha_final
    blue_final = blue_final_a / alpha_final
    # Put the color back together again!
    return Color.new(red_final, green_final, blue_final, alpha_final * 255)
  end
end

class Bitmap
  #=============================================================================
  # * drawCircle
  # drawCircle(color,radius,center_x,center_y,hollow?)
  # inori original example. It might not work.
  #-----------------------------------------------------------------------------
  def drawCircle(color=Color.new(255,255,255),r=(self.width/2),tx=(self.width/2),ty=(self.height/2),hollow=false)
    # basic circle formula
    # (x - tx)**2 + (y - ty)**2 = r**2
    pixels = self.raw_data.unpack('I*')
    colori = color.to_i
    for x in 0...self.width
      f = (r**2 - (x - tx)**2)
      row = self.height * x
      next if f < 0
      y1 = -Math.sqrt(f).to_i + ty
      y2 =  Math.sqrt(f).to_i + ty
      if hollow
        pixels[row + y1] = colori
        pixels[row + y2] = colori 
        #self.set_pixel(x,y1,color)
        #self.set_pixel(x,y2,color)
      else
        for y in y1..y2
          pixels[row + y] = colori
          #self.set_pixel(x,y,color)
        end
      end
    end
    self.raw_data = pixels.pack('I*')
  end
  
  #=============================================================================
  # * drawPolygon
  # drawPolygon(points,color,width)
  # I don't know how to fill polygons, sorry.
  #-----------------------------------------------------------------------------
  def drawPolygon(points, color=Color.new(255,255,255), width=1)
    # Invalid Width or points
    return if width < 1
    # Create bit table
    bits = Table.new(self.width, self.height)
    # Single point
    if (points.size == 1)
      x = points[0]
      y = points[1]
      
      if width==1
        bits[x,y] = 1
      else
        x1 = x - width/2
        x2 = x1 + width
        y1 = y - width/2
        y2 = y1 + width
        
        for xx in x1..x2
          for yy in y1..y2
            bits[xx,yy] = 1
          end
        end
      end
      
    else
      # Draw each line?
      for i in 0...(points.size-1)
        sP = points[i]
        eP = points[i+1]
        dirX = eP.x > sP.x ? 1 : -1
        dirY = eP.y > sP.y ? 1 : -1
        lenX = (eP.x-sP.x).abs
        lenY = (eP.y-sP.y).abs
        horz = lenX > lenY
        iters= horz ? lenX : lenY
        for i in 0...iters
          x = horz ? (sP.x + i * dirX) : sP.x + (lenX * i / iters)
          y = horz ? sP.y + (lenY * i / iters) : (sP.y + i * dirY)
          x = x.floor
          y = y.floor
          if width==1
            bits[x,y] = 1
          else
            x1 = x - width/2
            x2 = x1 + width
            y1 = y - width/2
            y2 = y1 + width
            
            for xx in x1..x2
              for yy in y1..y2
                bits[xx,yy] = 1
              end
            end
          end
        end
      end
    end
    # Submit changes
    drawRoutine(bits, color)
  end
  
  #=============================================================================
  # * drawRoutine
  # drawRoutine(bits,color)
  # Submits changes done in other subroutines. This way, even though makes it 
  # iterate through more data, "blits" only once per pixel. Helps with blending.
  #-----------------------------------------------------------------------------
  def drawRoutine(bits, color)
    # Get 
    pixels = self.raw_data.unpack('I*')
    col = Color.new
    # Iterate and blend pixels
    for y in 0..bits.xsize
      row = self.width * y
      for x in 0..bits.xsize
        if bits[x,y]==1
          pos = row + x
          col.from_i(pixels[pos])
          pixels[pos] = color.blend(col).to_i
        end
      end
    end
    # Submit
    self.raw_data = pixels.pack('I*')
  end
end