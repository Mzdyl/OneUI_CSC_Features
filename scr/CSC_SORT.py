import xml.etree.ElementTree as ET

def sort_feature_set(xml_file_path, output_file_path=None):
    """
    Sorts the content of the <FeatureSet> element in a Samsung Mobile Feature XML file
    alphabetically by tag name.
    
    Args:
        xml_file_path (str): Path to the input XML file
        output_file_path (str, optional): Path for the output XML file. If None, will overwrite input file.
    """
    # Parse the XML file
    tree = ET.parse(xml_file_path)
    root = tree.getroot()
    
    # Find the FeatureSet element
    feature_set = root.find('FeatureSet')
    
    if feature_set is None:
        print("Error: FeatureSet element not found in the XML file.")
        return
    
    # Get all children of FeatureSet
    features = list(feature_set)
    
    # Remove all features from FeatureSet
    for feature in features:
        feature_set.remove(feature)
    
    # Sort features by tag name
    sorted_features = sorted(features, key=lambda x: x.tag)
    
    # Add sorted features back to FeatureSet
    for feature in sorted_features:
        feature_set.append(feature)
    
    # Set output path
    if output_file_path is None:
        output_file_path = xml_file_path
    
    # Write the modified XML to the output file
    tree.write(output_file_path, encoding='utf-8', xml_declaration=True)
    print(f"XML file has been sorted and saved to: {output_file_path}")

# Example usage
if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else None
        sort_feature_set(input_file, output_file)
    else:
        print("Usage: python xml_sorter.py input_file.xml [output_file.xml]")
        print("If output_file.xml is not provided, the input file will be overwritten.")

