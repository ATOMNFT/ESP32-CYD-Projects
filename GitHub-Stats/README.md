<div align="center">
  
# GitHub stats Sketch

</div>

<b>Sample project that fetches and displays some basic stat info from a github repo of your choosing. 
<br>
The data is refreshed every minute or you can manually refresh it with the button on the screen. 
<br>
The rgb led on the back changes to blue upon a refresh.</b>

## Changing text color/s
If you would like to edit the colors of the text, the following bits of code are where you make these edits.
> void displayStats(String repoName, int stars, int forks, int issues, String lastCommit, int followers, int notificationsCount) { <br>
  tft.fillScreen(TFT_BLACK); // Clear screen <br>
  tft.setTextColor(TFT_VIOLET); // Edit for heading color <br>
  tft.setTextSize(2); <br>
  tft.setCursor(45, 10); <br>
  tft.println("GitHub Stats:"); <br>
  tft.setTextColor(TFT_YELLOW); // Edit for stats color
  
### Color choices

- TFT_BLACK       
- TFT_NAVY        
- TFT_DARKGREEN   
- TFT_DARKCYAN    
- TFT_MAROON      
- TFT_PURPLE      
- TFT_OLIVE       
- TFT_LIGHTGREY   
- TFT_DARKGREY    
- TFT_BLUE        
- TFT_GREEN       
- TFT_CYAN        
- TFT_RED         
- TFT_MAGENTA     
- TFT_YELLOW      
- TFT_WHITE       
- TFT_ORANGE      
- TFT_GREENYELLOW 
- TFT_PINK        
- TFT_BROWN       
- TFT_GOLD        
- TFT_SILVER      
- TFT_SKYBLUE     
- TFT_VIOLET    

___     
  
<br>

![atom-stats](images/atom-stats.jpg)
<br>
![fr4nkstatsstats](images/fr4nkstats.jpg)